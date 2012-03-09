import pyCatcherModel
from settings import Provider_list, Provider_Country_list

class RuleResult:
    OK = 'Ok'
    WARNING = 'Warning'
    CRITICAL = 'Critical'

class Rule:

    is_active = False
    identifier = 'Rule'

    def check(self, arfcn, base_station_list):
        return RuleResult.CRITICAL

class ProviderRule (Rule):

    identifier = 'Provider Check'

    def check(self, arfcn, base_station_list):
        result = RuleResult.CRITICAL
        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in Provider_list:
                    result = RuleResult.OK
                    break
        return result

class CountryProvider (Rule):
    pass

class BSICIntegrity (Rule):
    pass

class Uniqueness (Rule):
    pass

class NeighbourhoodStructure (Rule):
    pass

class LACIntegrity (Rule):
    pass

class CellIDDatabase (Rule):
    pass

class MachineLearning (Rule):
    pass