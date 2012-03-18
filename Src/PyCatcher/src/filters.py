class Filter:
    def __init__(self):
        self.is_active = False
        self.params = {}
        
    def execute(self, station_list):
        raise NotImplementedError('Filter not yet implemented')

class BandFilter(Filter):
    band = 0

    def execute(self, station_list):
        raise NotImplementedError('Band Filters should not be executed!')

class ARFCNFilter(Filter):
    def execute(self, station_list):
        filtered_list = []
        low = self.params['from']
        high = self.params['to']
        for station in station_list:
            if station.arfcn <= high and station.arfcn >= low:
                filtered_list.append(station) 
        return filtered_list
    
class ProviderFilter(Filter):
    def execute(self, station_list):
        filtered_list = []
        providers  = [x.strip() for x in self.params['providers'].split(',')]
        for station in station_list:
            if station.provider in providers:
                filtered_list.append(station) 
        return filtered_list

class BandFilter900(BandFilter):
    band = 900

