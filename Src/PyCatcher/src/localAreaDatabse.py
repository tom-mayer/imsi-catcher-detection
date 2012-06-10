import sqlite3
import os
from pyCatcherModel import BaseStationInformation
from settings import Database_path
class LocalAreaDatabase:

    def __init__(self):
        self._connection = None
        self._cursor = None
        self.cache = []

    def load_or_create_database(self, name):
        if self._connection:
            self._connection.close()
            self._connection = None

        name += '.db'
        path = os.path.join(Database_path ,name)
            
        database_exists = os.path.exists(path)
        self._connection = sqlite3.connect(path)
        self._cursor = self._connection.cursor()
        if not database_exists:
            self._create_base_table()

        self.refresh_object_cache()

    def _create_base_table(self):
        sql = '''CREATE TABLE basestations(
        cellid INTEGER, country TEXT, provider TEXT, arfcn INTEGER, bsic TEXT, lac INTEGER,
        rxmin INTEGER, rxmax INTEGER, sightings INTEGER
        )
        '''
        self._cursor.execute(sql)
        self._connection.commit()

    def _insert_station(self, base_station):
        values = (  base_station.cell,
                    base_station.country,
                    base_station.provider,
                    base_station.arfcn,
                    base_station.bsic,
                    base_station.lac,
                    base_station.rxlev,
                    base_station.rxlev,
                    base_station.times_scanned
                 )
        sql = 'INSERT INTO basestations VALUES (?,?,?,?,?,?,?,?,?)'
        self._cursor.execute(sql, values)
        self._connection.commit()

    def _alter_station(self, base_station, old_rmin, old_rmax, old_sightings):

        current_rx = int(base_station.rxlev)

        if current_rx < old_rmin:
            rmin = current_rx
        else:
            rmin = old_rmin
        if current_rx > old_rmax:
            rmax = current_rx
        else:
            rmax = old_rmax
        sightings = old_sightings + base_station.times_scanned
        values = (rmin,rmax,sightings,base_station.cell)
        sql = 'UPDATE basestations SET rxmin=?, rxmax=?, sightings=? WHERE cellid=?'
        self._cursor.execute(sql, values)
        self._connection.commit()

    def get_station(self, cellID):
        for item in self.cache:
            if item.cellID == cellID:
                return item
        return None

    def refresh_object_cache(self):
        if not self._connection:
            return
        self.cache = []
        sql = 'SELECT * FROM basestations'
        self._cursor.execute(sql)
        try:
            result = self._cursor.fetchall()
            for line in result:
                self.cache.append(
                    LACDBEntry(
                        line[0], line[1], line[2], line[3], line[4],
                        line[5], line[6], line[7],line[8]
                    )
                )
        except:
            print 'shouldnt happen'
            return

    def _get_station(self, cellID):
        if not self._connection:
            return None
        sql = 'SELECT * FROM basestations WHERE cellid =%d'%cellID
        self._cursor.execute(sql)
        try:
            result = (self._cursor.fetchall())[0]
        except:
            result = None
        return result

    def insert_or_alter_base_stations(self, base_station_list):
        for station in base_station_list:
            self.insert_or_alter_base_station(station)

    def insert_or_alter_base_station(self, base_station):
        lookupresult = self._get_station(base_station.cell)
        if lookupresult:
            self._alter_station(base_station,lookupresult[6],lookupresult[7],lookupresult[8])
        else:
            self._insert_station(base_station)

    def __del__(self):
        if self._cursor:
            self._cursor.close()
        if self._connection:
            self._connection.close()

class LACDBEntry():
    def __init__(self, cellID, country, provider, arfcn, bsic, lac, rxmin, rxmax, sightings):
        self.cellID = cellID
        self.country = country
        self.provider = provider
        self.arfcn = arfcn
        self.bsic = bsic
        self.lac = lac
        self.rxmin = rxmin
        self.rxmax = rxmax
        self.sightings = sightings











