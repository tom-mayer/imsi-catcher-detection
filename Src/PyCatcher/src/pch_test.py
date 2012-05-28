from settings import Commands
import subprocess
import time
import datetime

def main():

    pch_retries = 5
    max_scan_time = 20
    arfcn = 17
    pages_found = 0
    ia_non_hop_found = 0
    ia_hop_fund = 0

    command = Commands['pch_command'] + ['-a', str(arfcn)]
    scan_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    time.sleep(2)
    retry = False

    start_time = datetime.datetime.now()
    scan_time = datetime.datetime.now() - start_time

    while(True):
        if(retry):
            scan_process.terminate()
            scan_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            retry = False

        while(pch_retries > 0 and scan_time.seconds < max_scan_time):
            scan_time = datetime.datetime.now() - start_time
            line = scan_process.stdout.readline()
            if line:
                if 'Paging' in line:
                    pages_found += 1
                if 'IMM' in line:
                    if 'HOP' in line:
                        ia_hop_fund += 1
                    else:
                        ia_non_hop_found += 1
                if 'FBSB RESP: result=255' in line:
                    if(pch_retries > 0):
                        retry = True
                    break

        if(retry):
            pch_retries -= 1
        else:
            break

    if scan_process:
        scan_process.terminate()

    result = {
        'Pagings': pages_found,
        'Assignments_hopping': ia_hop_fund,
        'Assignments_non_hopping': ia_non_hop_found
    }

    print result

if __name__ == "__main__":
    main()