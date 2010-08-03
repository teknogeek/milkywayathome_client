/*****************************************************************************
 *                                                                           *
 *  Copyright (C) 2010 Shane Reilly, Ben Willet, Matthew Newby, Heidi        *
 *  Newberg, Malik Magdon-Ismail, Carlos Varela, Boleslaw Szymanski, and     *
 *  Rensselaer Polytechnic Institute                                         *
 *                                                                           *
 *  This file is part of the MilkyWay@Home Project.                          *
 *                                                                           *
 *  This program is free software: you can redistribute it and/or modify     *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation, either version 3 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the             *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                           *
 *  Shane Reilly                                                             *
 *  reills2@cs.rpi.edu                                                       *
 *                                                                           *
 *****************************************************************************/

#ifndef _DEMOFILE_HPP_
#define _DEMOFILE_HPP_

#include "binfile.hpp"


class NBodyFile
{

private:

    ifstream fstrm;
    string fileName;
    int starTotal;
    bool done, binFlag;

public:

    void reset()
    {
        if( fstrm.is_open() )
            fstrm.close();
        if( binFlag ) {
            fstrm.open(fileName.c_str(), ios::in|ios::binary);
            starTotal = fileGetIntBin(fstrm);
        }
        else {
            fstrm.open(fileName.c_str());
            starTotal = fileGetInt(fstrm);
        }
        done = false;

cout << starTotal << "*********" << endl;
    }

    NBodyFile( string fileName, bool binFlag = true )
    {
        this->binFlag = binFlag;
        this->fileName = fileName;
        reset();
    }

    int getStarTotal() { return starTotal; }

    bool readStars( HaloField& stream, double lum = .1 )

        // Reads next step in 'fstrm' into stream data
        // Returns true if another step exists, false if this is the last step in the file

    {

        if( done )
            return false;

        stream.clearField();

//cerr << "DEBUG: Location " << (unsigned long long) fstrm.tellg() << endl;

        // Confirm number of dimensions (must be 3)
        int dimensions;
        if( binFlag )
            dimensions = fileGetIntBin(fstrm);
        else
            dimensions = fileGetInt(fstrm);
        if( dimensions!=3 ) {
            cerr << "Location " << fstrm.tellg() << ": ";
            cerr << "Error reading file - dimensions must be 3 (was " << dimensions << ").\n" ;
            exit(1);
        }

        // Get elapsed time at current step in billions of years
        double elapsedTime;
        if( binFlag )
            elapsedTime = fileGetFloatBin(fstrm);
        else
            elapsedTime = fileGetDouble(fstrm);
/*
        // Eat masses
        if( binFlag )
            for( int i = 0; i<starTotal; i++ )
                fileGetFloatBin(fstrm);
*/
        // Get star positions and velocity vectors at current step
        float lineArg[3];
        for( int i = 0; i<starTotal; i++ ) {
            if( binFlag )
                fileGetFloatArrayBin(fstrm, 3, lineArg);
            else
                fileGetFloatArray(fstrm, 3, lineArg);
            stream.add(lineArg[0], lineArg[1], lineArg[2], lum, 120, 151);

        }

        // Skip over velocities since they are not used
        for( int i = 0; i<starTotal; i++ )
            if( binFlag )
                fileGetFloatArrayBin(fstrm, 3, lineArg);
            else
                fileGetFloatArray(fstrm, 3, lineArg);

        // Check to see if there is another step
        if( fstrm.eof() ) {
            fstrm.close();
            done = true;
        }
        else {

            // Check current star number against old number - exit if they don't match
            int starVerify;
            if( binFlag )
                starVerify = fileGetIntBin(fstrm);
            else
                starVerify = fileGetInt(fstrm);
            if( starTotal != starVerify ) {
                cerr << "Location " << fstrm.tellg() << ": ";
                cerr << "Error reading file - number of stars does not match initial value (" << starVerify << " != " << starTotal << ").\n";
                exit(1);
            }

        }

        return true;

    }

};


class WedgeFile
{

private:

    ifstream fstrm;
    string fileName;
    int starTotal;
    bool binFlag;

public:

    void reset()
    {
        starTotal = 0;
    }

    WedgeFile( bool binFlag = false )
    {
        this->binFlag = binFlag;
        reset();
    }

    int getStarTotal() { return starTotal; }

    int getStarTotal( string fileName )
    {

        int starTotal;

        if( binFlag ) {
            fstrm.open(fileName.c_str(), ios::in|ios::binary);
            starTotal = fileGetIntBin(fstrm);
        }
        else {
            fstrm.open(fileName.c_str());
            starTotal = fileGetInt(fstrm);
        }
        fstrm.close();

        return starTotal;

    }

    void readStars( string fileName, HaloField& field, double lum = .1 )

        // Reads next step in 'fstrm' into stream data
        // Returns true if another step exists, false if this is the last step in the file

    {

        int starTotal;

        if( binFlag ) {
            fstrm.open(fileName.c_str(), ios::in|ios::binary);
            starTotal = fileGetIntBin(fstrm);
        }
        else {
            fstrm.open(fileName.c_str());
            starTotal = fileGetInt(fstrm);
        }

        this->starTotal += starTotal;

        // Get star positions
        float lineArg[3];
        for( int i = 0; i<starTotal; i++ ) {

            if( binFlag )
                fileGetFloatArrayBin(fstrm, 3, lineArg);
            else
                fileGetFloatArray(fstrm, 3, lineArg);

            double l = lineArg[0];
            double b = lineArg[1];
            double r = lineArg[2];

            double x = r*cos(b*TRIG_DEG_TO_RAD)*cos(l*TRIG_DEG_TO_RAD);
            double y = r*cos(b*TRIG_DEG_TO_RAD)*sin(l*TRIG_DEG_TO_RAD);
            double z = r*sin(b*TRIG_DEG_TO_RAD);

            // Offset x to align with galactic center
            x -= 8.;

            field.add(x, y, z, lum, 64, 45);

            if( fstrm.eof() ) {
                fstrm.close();
                if( starTotal!=i ) {
                    cerr << "Number of stars indicated in wedge file does not match data." << endl;
                    exit(1);
                }
            }

        }

        /// TODO /// Check to see if there is more data in the file (use a look ahead perhaps to avoid doubling the error checking)
/*      if( !fstrm.eof() ) {
            fstrm.close();
            cerr << "Number of stars indicated in wedge file does not match data." << endl;
            exit(1);
        }
*/

        fstrm.close();

    }

};


#endif /* _DEMOFILE_HPP_ */
