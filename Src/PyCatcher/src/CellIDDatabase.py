import re
import urllib2
from settings import Open_Cell_ID_Key
from struct import pack, unpack
from httplib import HTTP

class Translator:
    Country = {
        'Germany':'262'
    }

    Provider = {
        'T-Mobile':'01',
        'Vodafone':'02',
        'E-Plus':'03',
        'O2':'07'
    }

    MCC = {
        262:'de'
    }

class CellIDDBStatus:
    CONFIRMED = 0
    APPROXIMATED = 1
    ERROR = 2
    NOT_LOOKED_UP = 3
    NOT_IN_DB = 4

class CellIDDatabaseFetcher:


    def fetch(self, cid, lac, mcc, mnc):
        print CID.fetch_id_from_Google(cid,lac,mcc)
        print CID.fetch_id_from_OpenCellID(cid,lac,mcc,mnc)


    def fetch_id_from_OpenCellID(self,cid, lac, mcc, mnc):
        key_ocid = Open_Cell_ID_Key

        url = 'http://www.opencellid.org/cell/get?key=%s&mnc=%d&mcc=%d&lac=%d&cellid=%d'%(key_ocid, mnc, mcc, lac, cid)
        response = urllib2.urlopen(url).read()

        status = (re.search(r'stat="(.+)"',response)).group(1)

        if status != 'ok':
            status = CellIDDBStatus.ERROR
            return

        match = re.search(r'lat="(\d+\.\d+)".*lon="(\d+\.\d+).*range="(\d+)"',response)
        latitude,longitude,range = match.group(1),match.group(2),match.group(3)

        if int(range) > 10000:
            status = CellIDDBStatus.APPROXIMATED
        else:
            status = CellIDDBStatus.CONFIRMED

        latitude = float(latitude)
        longitude = float(longitude)

        if latitude == 0 or longitude == 0:
            status = CellIDDBStatus.NOT_IN_DB

        return status, latitude, longitude


    def fetch_id_from_Google(self, cid, lac, country):
        device = "Motorola C123"
        country = Translator.MCC[country]
        b_string = pack('>hqh2sh13sh5sh3sBiiihiiiiii',
            21, 0,
            len(country), country,
            len(device), device,
            len('1.3.1'), "1.3.1",
            len('Web'), "Web",
            27, 0, 0,
            3, 0, cid, lac,
            0, 0, 0, 0)

        http = HTTP('www.google.com', 80)
        http.putrequest('POST', '/glm/mmap')
        http.putheader('Content-Type', 'application/binary')
        http.putheader('Content-Length', str(len(b_string)))
        http.endheaders()
        http.send(b_string)
        code, msg, headers = http.getreply()
        try:
            bytes = http.file.read()
            (a, b,errorCode, latitude, longitude, c, d, e) = unpack(">hBiiiiih",bytes)
            latitude = latitude / 1000000.0
            longitude = longitude / 1000000.0
            status = CellIDDBStatus.CONFIRMED
        except:
            status = CellIDDBStatus.NOT_IN_DB

        return status, latitude, longitude