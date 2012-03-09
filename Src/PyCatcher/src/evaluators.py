from rules import RuleResult

class EvaluatorSelect:
    CONSERVATIVE = 0
    WEIGHTED = 1
    BAYES = 2

class Evaluator:

    return_type = type(RuleResult)

    def evaluate(self, result_list):
        return RuleResult.CRITICAL

class ConservativeEvaluator(Evaluator):

    def evaluate(self, result_list):
        final_result = RuleResult.OK
        for key in result_list.keys():
            if result_list[key] == RuleResult.WARNING:
                final_result = RuleResult.WARNING
            if result_list[key] == RuleResult.CRITICAL:
                final_result = RuleResult.CRITICAL
                break
        return final_result

class BayesEvaluator(Evaluator):
    return_type = type(int)

class WeightedEvaluator(Evaluator):
    return_type = type(int)