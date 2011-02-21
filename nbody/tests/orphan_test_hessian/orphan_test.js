{  //test results are using double
"nbody-parameters-file": {
    "nbody-context": {
        "headline" : "disk=miyamoto-nagai_halo=logarithmic_quad=true_criterion=sw93",
        "criterion" : "sw93",
        "use-quadrupole-corrections" : true,
        "accuracy-parameter" : 1.0,
        "seed" : 25,

        "potential" : {
            "disk" : {
                "miyamoto-nagai" : {
                    "mass" : 4.45865888E5,
                    "scale-length" : 6.5,
                    "scale-height" : 0.26
                }
            },

            "spherical" : {
                "sphere" : {
                    "mass" : 1.52954402E5,
                    "r0-scale" : 0.7
                }
            },

            "halo" : {
                "logarithmic" : {
                    "vhalo" : 73,
                    "scale-length" : 12.0,
                    "z-flattening" : 1.0,
                }
            }
        },

        "dwarf-model": {
            "plummer" : {
            "mass" : 12.3820039691,
            "nbody" : 1024,
            "scale-radius" : 0.685695643277,
            "time-orbit" : 4.058745708,
            "time-dwarf" : 3.89893414985,
            }
        }
    },

    "initial-conditions": {
        "useGalC" : false,
        "angle-use-radians" : false,
        "velocity" : [ -156, 79, 107 ],
        "position" : [ 218, 53.5, 28.6 ]
    },


}}

