#Core Configuration ------------------------------------------------------------------------------------------

PyCatcher_settings = {'debug' : True,
                    }

Device_settings = { 'mobile_device' : '/dev/ttyUSB0',
                    'xor_type' : 'c123xor',
                    'firmware' : 'compal_e88',
                   }

Osmocon_lib = '/home/tom/imsi-catcher-detection/Src/osmolib/src'

Commands = {'osmocon_command' : [Osmocon_lib + '/host/osmocon/osmocon', 
                                    '-p', Device_settings['mobile_device'], 
                                    '-m', Device_settings['xor_type'], 
                                    Osmocon_lib + '/target/firmware/board/' + Device_settings['firmware']
                                    + '/layer1.compalram.bin'],
            'scan_command' : [Osmocon_lib + '/host/layer23/src/misc/catcher'],
            'pch_command' : [Osmocon_lib + '/host/layer23/src/misc/pch_scan'],
           }

#Rules Configuration -------------------------------------------------------------------------------------------

Provider_list = ['T-Mobile', 'O2', 'Vodafone', 'E-Plus']

Provider_Country_list = {
    'DB Systel GSM-R':'Germany',
    'T-Mobile':'Germany',
    'O2':'Germany',
    'Vodafone':'Germany',
    'E-Plus':'Germany'
}

LAC_mapping = {
    'DB Systel GSM-R': [0],
    'T-Mobile' : [21013,21014,21015],
    'O2' : [50945],
    'Vodafone' : [793],
    'E-Plus' : [138,588]
}

ARFCN_mapping = {
    'DB Systel GSM-R': [(0,1)],
    'T-Mobile' : [(13,49),(81, 102),(122,124),(587,611)],
    'O2' : [(0,0),(1000,1023),(637,723)],
    'Vodafone' : [(1,12),(50,80),(103,121),(725,751)],
    'E-Plus' : [(975,999),(777,863)]
}

LAC_threshold = 0.05

DB_RX_threshold = 0.1

CH_RX_threshold = 0.2

Pagings_per_10s_threshold = 20

Assignment_limit = 0

Neighbours_threshold = -1

#Evaluator Configuration ---------------------------------------------------------------------------------------

Rule_Groups = [
    ['Provider Check', 'Country Provider Mapping', 'ARFCN Mapping', 'LAC Mapping', 'Unique CellID'],
    ['LAC Median Deviation', 'Neighbourhood Structure', 'Pure Neighbourhoods', 'Fully Discovered Neighbourhoods'],
    ['Local Area Database','CellID Database'],
    ['LAC Change Rule','rx Change Rule'],
    ['PCH Scan']
]

#PCH Parameters ------------------------------------------------------------------------------------------------

PCH_retries = 5

USR_timeout = 15

#Database Configuration ----------------------------------------------------------------------------------------

Open_Cell_ID_Key = 'd7a5bc3f21b44d4bf93d1ec2b3f83dc4'

Database_path = '/home/tom/imsi-catcher-detection/Src/PyCatcher/Databases/'

