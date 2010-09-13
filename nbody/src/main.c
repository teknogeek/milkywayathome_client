/* Copyright 2010 Matthew Arsenault, Travis Desell, Boleslaw
Szymanski, Heidi Newberg, Carlos Varela, Malik Magdon-Ismail and
Rensselaer Polytechnic Institute.

This file is part of Milkway@Home.

Milkyway@Home is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Milkyway@Home is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Milkyway@Home.  If not, see <http://www.gnu.org/licenses/>.
*/

/* TODO: wuh wuh windows */
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <popt.h>
#include <errno.h>

#include <json/json.h>

#include "nbody_config.h"
#include "milkyway_util.h"
#include "nbody.h"

#ifdef _WIN32
  #define R_OK 4 /* FIXME: Windows */
#endif


#define DEFAULT_CHECKPOINT_FILE "nbody_checkpoint"
#define DEFAULT_HISTOGRAM_FILE  "histogram"


/* If one of these options is null, use the default. */
#define stringDefault(s, d) ((s) = (s) ? (s) : strdup((d)))


#if !BOINC_APPLICATION
static void nbodyBoincInit() { }
#else

#if BOINC_DEBUG
/* Use BOINC, but prevent it from redirecting stderr to a file, which
 * is really annoying for debugging */
static void nbodyBoincInit()
{
    int rc =  boinc_init_diagnostics(  BOINC_DIAG_DUMPCALLSTACKENABLED
                                     | BOINC_DIAG_HEAPCHECKENABLED
                                     | BOINC_DIAG_MEMORYLEAKCHECKENABLED);
    if (rc)
    {
        warn("boinc_init failed: %d\n", rc);
        exit(EXIT_FAILURE);
    }
}

#else
/* For BOINC releases */
static void nbodyBoincInit()
{
    int rc = boinc_init();
    if (rc)
    {
        warn("boinc_init failed: %d\n", rc);
        exit(EXIT_FAILURE);
    }
}
#endif /* BOINC_DEBUG */
#endif /* !BOINC_APPLICATION */


/* Maybe set up some platform specific issues */
static void specialSetup()
{
  #if !defined(__SSE2__) && ENABLE_CRLIBM
    /* Try to handle inconsistencies with x87. We shouldn't use
     * this. This helps, but there can still be some problems for some
     * values. Sticking with SSE2 is the way to go. */
    crlibm_init();
  #endif

  #ifdef _WIN32
    /* Make windows printing be more consistent. For some reason it
     * defaults to printing 3 digits in the exponent. There are still
     * issues where the rounding of the last digit by printf on
     * windows in a small number of cases. */
    _set_output_format(_TWO_DIGIT_EXPONENT);
  #endif /* _WIN32 */
}

#if BOINC_APPLICATION

/* Read JSON from a file using BOINC file functions */
static json_object* nbodyJSONObjectFromFile(const char* inputFile)
{
    char resolvedPath[1024];
    int rc;
    char* buf;
    json_object* obj;

    rc = boinc_resolve_filename(inputFile, resolvedPath, sizeof(resolvedPath));
    if (rc)
        fail("Error resolving file '%s': %d\n", inputFile, rc);

    buf = mwReadFile(resolvedPath);
    if (!buf)
    {
        warn("Failed to read resolved path '%s'\n", resolvedPath);
        return NULL;
    }

    obj = json_tokener_parse(buf);
    free(buf);
    return obj;
}

#else

static json_object* nbodyJSONObjectFromFile(const char* inputFile)
{
    return json_object_from_file(inputFile);
}

#endif /* BOINC_APPLICATION */


static void setFitParams(FitParams* fitParams, const real* parameters)
{
    fitParams->modelMass        = parameters[0];
    fitParams->modelRadius      = parameters[1];
    fitParams->reverseOrbitTime = parameters[2];
    fitParams->simulationTime   = parameters[3];
}

static json_object* readJSONFileOrStr(const char* inputFile, const char* inputStr)
{
    json_object* obj         = NULL;
    if (inputFile)
    {
        /* Check if we can read the file, so we can fail saying that
         * and not be left to guessing if it's that or a parse
         * error */
        if (access(inputFile, R_OK) < 0)
        {
            perror("Failed to read input file");
            return NULL;
        }

        /* The lack of parse errors from json-c is unfortunate.
           TODO: If we use the tokener directly, can get them.
         */

        obj = nbodyJSONObjectFromFile(inputFile);
        if (is_error(obj))
        {
            warn("Parse error in file '%s'\n", inputFile);
            obj = NULL;
        }
    }
    else
    {
        obj = json_tokener_parse(inputStr);
        if (is_error(obj))
        {
            warn("Failed to parse given string\n");
            return NULL;
        }
    }

    return obj;
}

static int handleServerArguments(FitParams* fitParams, const char** rest, const unsigned int numParams)
{
    unsigned int i;
    real* parameters = NULL;
    unsigned int paramCount = 0;

    fitParams->useFitParams = TRUE;

    /* Read through all the server arguments, and make sure we can
     * read everything and have the right number before trying to
     * do anything with them */

    while (rest[++paramCount]);  /* Count number of parameters */

    if (numParams == 0)
    {
        warn("numParams = 0 makes no sense\n");
        return 1;
    }

    /* Make sure the number of extra parameters matches the number
     * we were told to expect. */
    if (numParams != paramCount)
    {
        warn("Parameter count mismatch: Expected %u, got %u\n", numParams, paramCount);
        return 1;
    }

    parameters = (real*) mallocSafe(sizeof(real) * numParams);

    errno = 0;
    for (i = 0; i < numParams; ++i)
    {
        parameters[i] = (real) strtod(rest[i], NULL);
        if (errno)
        {
            perror("Error parsing command line fit parameters");
            free(parameters);
            return 1;
        }
    }

    setFitParams(fitParams, parameters);
    free(parameters);

    return 0;
}


/* Read the command line arguments, and do the inital parsing of the parameter file. */
static json_object* readParameters(const int argc,
                                   const char** argv,
                                   FitParams* fitParams,
                                   NBodyFlags* nbf)
{
  #if !BOINC_APPLICATION
    #pragma unused(checkpointFileName)
    #pragma unused(ignoreCheckpoint)
  #endif

    poptContext context;
    int o;
    json_object* obj         = NULL;
    static char* inputFile   = NULL;   /* input JSON file */
    static char* inputStr    = NULL;   /* a string of JSON to use directly */
    const char** rest        = NULL;   /* Leftover arguments */
    int failed = FALSE;

    unsigned int numParams = 0, params = 0;

    /* FIXME: There's a small leak of the inputFile from use of
       poptGetNextOpt(). Some mailing list post suggestst that this
       is some kind of semi-intended bug to work around something or
       other while maintaining ABI compatability */
    const struct poptOption options[] =
    {
        {
            "input-file", 'f',
            POPT_ARG_STRING, &inputFile,
            0, "Input file to read", NULL
        },

        {
            "histoout-file", 'z',
            POPT_ARG_STRING, &nbf->histoutFileName,
            0, "Output histogram file", NULL
        },

        {
            "histogram-file", 'h',
            POPT_ARG_STRING, &nbf->histogramFileName,
            0, "Histogram file", NULL
        },

        {
            "output-file", 'o',
            POPT_ARG_STRING, &nbf->outFileName,
            0, "Output file", NULL
        },

        {
            "input-string", 's',
            POPT_ARG_STRING, &inputStr,
            0, "Input given as string", NULL
        },

        {
            "output-cartesian", 'x',
            POPT_ARG_NONE, &nbf->outputCartesian,
            0, "Output Cartesian coordinates instead of lbR", NULL
        },

        {
            "timing", 't',
            POPT_ARG_NONE, &nbf->printTiming,
            0, "Print timing of actual run", NULL
        },

        {
            "check-file", 'g',
            POPT_ARG_NONE, &nbf->verifyOnly,
            0, "Check that the input file is valid only; perform no calculation.", NULL
        },

      #if BOINC_APPLICATION
        {
            "checkpoint", 'c',
            POPT_ARG_STRING, &nbf->checkpointFileName,
            0, "Checkpoint file to use", NULL
        },

        {
            "clean-checkpoint", 'k',
            POPT_ARG_NONE, &nbf->cleanCheckpoint,
            0, "Cleanup checkpoint after finishing run", NULL
        },

        {
            "ignore-checkpoint", 'i',
            POPT_ARG_NONE, &nbf->ignoreCheckpoint,
            0, "Ignore the checkpoint file", NULL
        },

        {
            "print-bodies", 'b',
            POPT_ARG_NONE, &nbf->printBodies,
            0, "Print bodies", NULL
        },

        {
            "print-histogram", 'm',
            POPT_ARG_NONE, &nbf->printHistogram,
            0, "Print histogram", NULL
        },

        {
            "p", 'p',
            POPT_ARG_NONE, &params,
            0, "Unused dummy argument to satisfy primitive arguments the server sends", NULL
        },

        {
            "np", '\0',
            POPT_ARG_INT | POPT_ARGFLAG_ONEDASH, &numParams,
            0, "Unused dummy argument to satisfy primitive arguments the server sends", NULL
        },

        {  /* FIXME: Only used when using the server arguments. */
            "seed", 'e',
            POPT_ARG_INT, &nbf->setSeed,
            0, "seed for PRNG", NULL
        },

      #endif /* BOINC_APPLICATION */

        POPT_AUTOHELP

        { NULL, 0, 0, NULL, 0, NULL, NULL }
    };

    context = poptGetContext(argv[0],
                             argc,
                             argv,
                             options,
                             POPT_CONTEXT_POSIXMEHARDER);

    if (argc < 2)
    {
        poptPrintUsage(context, stderr, 0);
        poptFreeContext(context);
        mw_finish(EXIT_FAILURE);
    }

    while ( (o = poptGetNextOpt(context)) >= 0 );

    /* Check for invalid options, and must have one of input file or input string */
    if (     o < -1
         ||  (inputFile && inputStr)
         || !(inputFile || inputStr))
    {
        poptPrintHelp(context, stderr, 0);
        failed = TRUE;
    }

    if (params)
    {
        rest = poptGetArgs(context);
        if (!rest)
        {
            warn("Expected arguments to follow, got 0\n");
            failed = TRUE;
        }

        if (handleServerArguments(fitParams, rest, numParams))
        {
            warn("Failed to read server arguments\n");
            poptPrintHelp(context, stderr, 0);
            failed = TRUE;
        }
    }

    if (!failed)
        obj = readJSONFileOrStr(inputFile, inputStr);

    poptFreeContext(context);
    free(inputFile);
    free(inputStr);

    return obj;
}

/* main: toplevel routine for hierarchical N-body code. */
int main(int argc, const char* argv[])
{
    char* outFile        = NULL;
    json_object* obj     = NULL;
    NBodyFlags nbf       = EMPTY_NBODY_FLAGS;
    FitParams fitParams  = EMPTY_FIT_PARAMS;

    specialSetup();
    nbodyBoincInit();

    obj = readParameters(argc, argv, &fitParams, &nbf);

    /* Use default if checkpoint file not specified */
    stringDefault(nbf.checkpointFileName, DEFAULT_CHECKPOINT_FILE);
    stringDefault(nbf.histogramFileName,  DEFAULT_HISTOGRAM_FILE);

    if (outFile)
        nbf.printBodies = TRUE;
    if (nbf.histoutFileName)    /* Specifying output files implies using them */
        nbf.printHistogram = TRUE;

    if (obj)
        runNBodySimulation(obj, &fitParams, &nbf);
    else
        fail("Failed to read parameters\n");

    if (nbf.cleanCheckpoint)
    {
        warn("Removing checkpoint file '%s'\n", nbf.checkpointFileName);
        mw_remove(nbf.checkpointFileName);
    }

    free(nbf.outFileName);
    free(nbf.checkpointFileName);
    free(nbf.histogramFileName);
    free(nbf.histoutFileName);
    mw_finish(EXIT_SUCCESS);
}

