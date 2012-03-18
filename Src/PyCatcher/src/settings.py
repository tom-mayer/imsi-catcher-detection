#Core Configuration ------------------------------------------------------------------------------------------

PyCatcher_settings = {'debug' : False,
                    }

Device_settings = { 'mobile_device' : '/dev/ttyUSB0',
                    'xor_type' : 'c123xor',
                    'firmware' : 'compal_e88',
                   }

Osmocon_lib = '/home/tom/imsi-catcher-detection/Src/osmolib/src'

Commands = {'osmocon_command' : [Osmocon_lib + '/host/osmocon/osmocon', 
                                    '-p', Device_settings['mobile_device'], 
                                    '-m', Device_settings['xor_type'], 
                                    Osmocon_lib + '/target/firmware/board/' + Device_settings['firmware'] + '/layer1.compalram.bin'],   
            'scan_command' : [Osmocon_lib + '/host/layer23/src/misc/catcher'],
           }

#Rules Configuration -------------------------------------------------------------------------------------------

Provider_list = ['T-Mobile', 'O2', 'Vodafone', 'E-Plus']

Provider_Country_list = {
    'T-Mobile':'Germany',
    'O2':'Germany',
    'Vodafone':'Germany',
    'E-Plus':'Germany'
}

LAC_mapping = {
    'T-Mobile' : [21000,22000],
    'O2' : [0,9999],
    'Vodafone' : [0,100000],
    'E-Plus' : [0,100000]
}

ARFCN_mapping = {
    'T-Mobile' : [0,9999],
    'O2' : [0,9999],
    'Vodafone' : [0,9999],
    'E-Plus' : [0,9999]
}

LAC_threshold = 0.5

BSIC_database = ''

Cell_ID_database = ''

Home_database = ''

#Evaluator Configuration ---------------------------------------------------------------------------------------

Bayes_probabilities = {}
Bayes_database = ''