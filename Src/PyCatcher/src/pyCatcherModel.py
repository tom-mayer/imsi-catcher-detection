import datetime

class BaseStationInformation: 
    def __init__ (self):
        self.arcfn = 0
        self.bsic = 0
        self.rxlev = 0
        self.s1 = []
        self.s2 = []
        self.s2bis = []
        self.s2ter = []
        self.s3 = []
        self.s4 = []
        self.neighbours = []
        
    def parse_file(self, fileobject):
        self.arcfn = fileobject.readline()
        self.bsic = fileobject.readline()
        self.rxlev = fileobject.readline()
        self.s1 = fileobject.readline().split(' ')
        self.s2 = fileobject.readline().split(' ')
        self.s2bis = fileobject.readline().split(' ')
        self.s2ter = fileobject.readline().split(' ')
        self.s3 = fileobject.readline().split(' ')
        self.s4 = fileobject.readline().split(' ')
        return self

    def get_tree_data(self):
        return [self.bsic,  self.arcfn, self.rxlev, datetime.datetime.now().strftime("%H:%M:%S")]
        
class BaseStationInformationList:
    def __init__(self):
        self.base_station_list = []
        
    def add_station(self):
        pass
    
    def get_dot_code(self):
        pass