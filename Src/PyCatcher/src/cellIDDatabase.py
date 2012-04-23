import re
import urllib2
from settings import Open_Cell_ID_Key
from struct import pack, unpack
from httplib import HTTP
import sqlite3
import os
from settings import Database_path

class CIDDatabases:
    NONE = 'None'
    GOOGLE = 'Google'
    OPENCID = 'Open Cell ID'
    LOCAL = 'Local'

class Translator:
    Country = {
        'Germany':'de'
    }

    Provider = {
        'T-Mobile':'01',
        'Vodafone':'02',
        'E-Plus':'03',
        'O2':'07'
    }

    MCC = {
        'Germany':262
    }

class CellIDDBStatus:
    CONFIRMED = 'Confirmed'
    APPROXIMATED = 'Approximated'
    ERROR = 'Error'
    NOT_LOOKED_UP = 'Not looked up'
    NOT_IN_DB = 'Not in DB.'

class CellIDDatabase:


    def fetch_id_from_OpenCellID(self,cid, lac, country, provider):
        key_ocid = Open_Cell_ID_Key

        mcc = Translator.MCC[country]
        mnc = Translator.Provider[provider]
        
        url = 'http://www.opencellid.org/cell/get?key=%s&mnc=%s&mcc=%d&lac=%d&cellid=%d'%(key_ocid,mnc,mcc,lac,cid)
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
        latitude = 0
        longitude = 0
        device = "Motorola C123"
        country = Translator.Country[country]
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
            latitude /= 1000000.0
            longitude /= 1000000.0
            status = CellIDDBStatus.CONFIRMED
        except:
            status = CellIDDBStatus.NOT_IN_DB

        return status, latitude, longitude

    def fetch_id_from_local(self, cid, database):
        database += '.db'
        path = os.path.join(Database_path, database)
        if os.path.exists(path):
            connection = sqlite3.connect(path)
        else:
            return CellIDDBStatus.ERROR,0,0
        if connection:
            cursor = connection.cursor()
        else:
            return CellIDDBStatus.ERROR,0,0

        sql = 'SELECT * FROM basestations WHERE cellid =%d'%cid
        cursor.execute(sql)
        try:
            result = (cursor.fetchall())[0]
        except:
            result = None

        if result:
            cursor.close()
            connection.close()
            return CellIDDBStatus.CONFIRMED,0,0
        else:
            cursor.close()
            connection.close()
            return CellIDDBStatus.NOT_IN_DB,0,0