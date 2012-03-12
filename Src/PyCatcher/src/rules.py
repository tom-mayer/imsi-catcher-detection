from settings import Provider_list, Provider_Country_list, LAC_mapping, ARFCN_mapping

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

class CountryMappingRule (Rule):
    identifier = 'Country Provider Mapping'

    def check(self, arfcn, base_station_list):
        result = RuleResult.OK
        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in Provider_Country_list:
                    if station.country != Provider_Country_list[station.provider]:
                        result = RuleResult.CRITICAL
                else:
                    result = RuleResult.CRITICAL
        return result

class ARFCNMappingRule (Rule):
    identifier = 'ARFCN Mapping'

    def check(self, arfcn, base_station_list):
        result = RuleResult.CRITICAL
        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in ARFCN_mapping:
                    if ARFCN_mapping[station.provider][0] < station.arfcn < ARFCN_mapping[station.provider][1]:
                        result = RuleResult.OK
        return result

class LACMappingRule (Rule):
    identifier = 'LAC Mapping'

    def check(self, arfcn, base_station_list):
        result = RuleResult.CRITICAL
        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in LAC_mapping:
                    if LAC_mapping[station.provider][0] < station.lac < LAC_mapping[station.provider][1]:
                        result = RuleResult.OK
        return result

class LACIntegrityRule (Rule):
    identifier = 'LAC Integrity'

class UniqueCellIDRule (Rule):
    identifier = 'Unique CellID'

    def check(self, arfcn, base_station_list):
        result = RuleResult.OK
        cell_id = 0
        for station in base_station_list:
            if station.arfcn == arfcn:
                cell_id = station.cell
        for station in base_station_list:
            if station.arfcn != arfcn:
                if station.cell == cell_id:
                    result = RuleResult.CRITICAL
        return result

class NeighbourhoodStructureRule (Rule):
    identifier = 'Neighbourhood Structure'

class CellIDDatabaseRule (Rule):
    identifier = 'CellID Database'

class BDDLearningRule (Rule):
    identifier = 'BDD Learning'