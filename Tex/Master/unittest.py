#!/usr/bin/python2.7

import re

rules = [
    { 'regexps':    [r" (\w+) \1 "],
      'correction': r"no repeated word" },

    { 'regexps':    ["i\.e\.", "ie\."],
      'correction': r"\ie" },

    { 'regexps':    ["e\.g\.", "eg\."],
      'exceptions': ["peg\."],
      'correction': r"\eg" },

    { 'regexps':    ["c\.f\.", "cf\."],
      'correction': r"\cf" },

    { 'regexps':    [", that", ", such that"],
      'correction': "no comma before that" },

    { 'regexps':    [", because"],
      'correction': "probably no comma before because (only if it is needed for disambiguation or if the clause is not essential)" },

    { 'regexps':    [": [A-Z][a-z]"],
      'exceptions': [r"\\(sub)*section\{.*\}"],
      'case sensitive' : True,
      'correction': "don't capitalize after colon" },

    { 'regexps':    ["^[A-Z][a-z]"],
      'last line':  [r":$"],
      'case sensitive' : True,
      'correction': "don't capitalize after colon" },

    { 'regexps':    [r"\\vert"],
      'correction': r"\mid" },

    { 'regexps':    [",[ ]*&", "&[ ]*,"],
      'correction': r"no comma in cases environment" },

]

for rule in rules:
    if rule.has_key('case sensitive') and rule['case sensitive']:
        sensitivity = 0
    else:
        sensitivity = re.IGNORECASE
    rule['regexps'] = [re.compile(regex, sensitivity) for regex in rule['regexps']]
    if not rule.has_key('exceptions'):
        rule['exceptions'] = []
    rule['exceptions'] = [re.compile(regex, sensitivity) for regex in rule['exceptions']]
    if not rule.has_key('last line'):
        rule['last line'] = []
    rule['last line'] = [re.compile(regex, sensitivity) for regex in rule['last line']]

re_todoline = re.compile(r"(.*)\\todo\{[^}]*\}(.*)")


def check_line(line, last_line):
    if line.strip().startswith("%"):
        return []
    m = re_todoline.match(line)
    if m:
        line = m.group(1) + m.group(2)
    corrections = []
    for rule in rules:
        for regex in rule['regexps']:
            s = regex.search(line)
            if s:
                if rule['last line']:
                    # has to have a match in the last line too
                    for regex in rule['last line']:
                        if regex.search(last_line):
                            break
                    else:
                        break
                for regex in rule['exceptions']:
                    s_except = regex.search(line)
                    if s_except:
                        break
                else:
                    corrections.append((s.group(0), rule['correction']))
    return corrections

def check_file(filename):
    last_line = ""
    n = 0
    print filename
    for i, line in enumerate(open(filename)):
        corrections = check_line(line, last_line)
        n += len(corrections)
        if corrections:
            print
            print "    Line %d: %s" % (i+1, line.strip())
            for (error, correction) in corrections:
                print "        '%s' should be '%s'." % (error, correction)
        last_line = line
    return n

def main():
    n = 0
    for line in open("Master.tex"):
        if line.strip().startswith("%"):
            continue
        m = re.search(r"\input\{(.+)\}", line)
        if not m:
            continue
        filename = m.group(1)
        if filename.startswith("header"):
            continue
        n += check_file(filename + ".tex")
    print
    print n, "Warnings"


if __name__ == "__main__":
    main()
