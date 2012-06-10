from rules import RuleResult
from settings import Rule_Groups

class EvaluatorSelect:
    CONSERVATIVE = 0
    GROUP = 2

class Evaluator:

    return_type = type(RuleResult)
    identifier = 'Base Class'

    def evaluate(self, result_list):
        return RuleResult.CRITICAL, {'Evaluator Base Class':'This should not happen!'}

class ConservativeEvaluator(Evaluator):

    identifier = 'Conservative Evaluator'

    def evaluate(self, result_list):
        final_result = RuleResult.OK
        decision_rule = 'None'
        for key in result_list.keys():
            if result_list[key] == RuleResult.WARNING:
                final_result = RuleResult.WARNING
                decision_rule = key
            if result_list[key] == RuleResult.CRITICAL:
                final_result = RuleResult.CRITICAL
                decision_rule = key
                break
        return final_result, {'Decision founded on': decision_rule}


class GroupEvaluator(Evaluator):
    identifier = 'Group Evaluator'

    def evaluate(self, result_list):
        group_results = []
        for group in Rule_Groups:
            group_results.append(self.evaluate_group_results(self.convert_to_group_result_list(group,result_list)))

        criticals = group_results.count(RuleResult.CRITICAL)
        warnings = group_results.count(RuleResult.WARNING)
        oks = group_results.count(RuleResult.OK)

        if criticals > 0:
            return RuleResult.CRITICAL,{'Criticals': criticals, 'Warnings': warnings, 'Oks':oks}
        elif warnings > 0:
            return RuleResult.WARNING,{'Criticals': criticals, 'Warnings': warnings, 'Oks':oks}
        elif oks > 0:
            return RuleResult.OK,{'Criticals': criticals, 'Warnings': warnings, 'Oks':oks}
        else:
            return RuleResult.CRITICAL,{'Reason': 'No evaluation possible, all active rules yield IGNORE.'}

    def convert_to_group_result_list(self, group, result_list):
        group_result_list = []
        for rule in group:
            if result_list.has_key(rule):
                group_result_list.append(result_list[rule])
        return group_result_list

    def evaluate_group_results(self, results):
        oks = results.count(RuleResult.OK)
        warnings = results.count(RuleResult.WARNING)
        criticals = results.count(RuleResult.CRITICAL)
        if criticals >= oks and criticals >= warnings and not criticals == 0:
            return RuleResult.CRITICAL
        elif warnings >= oks and warnings>= criticals and not warnings == 0:
            return RuleResult.WARNING
        elif oks >= criticals and oks >= warnings and not oks == 0:
            return RuleResult.OK
        else:
            return RuleResult.IGNORE