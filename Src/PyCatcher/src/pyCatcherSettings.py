#needed commands with full path to applications

PyCatcher_settings = {'debug' : False,
                    }

Device_settings = { 'mobile_device' : '/dev/ttyUSB0',
                    'xor_type' : 'c123xor',
                    'firmware' : 'compal_e88',
                   }

Osmocon_lib = '/home/tom/Documents/imsi-catcher-detection/Src/osmocom-bb/src'

Commands = {'osmocon_command' : [Osmocon_lib + '/host/osmocon/osmocon', 
                                    '-p', Device_settings['mobile_device'], 
                                    '-m', Device_settings['xor_type'], 
                                    Osmocon_lib + '/target/firmware/board/' + Device_settings['firmware'] + '/layer1.compalram.bin'],   
            'scan_command' : [Osmocon_lib + '/host/layer23/src/misc/catcher'],
           }
