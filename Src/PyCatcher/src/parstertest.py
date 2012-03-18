def si_to_bin(si):
    system_information = si.split(' ')
    neighbours = system_information[3:19]
    bin_representation = ''
    for value in neighbours:
        bin_representation += str(bin(int(value, 16))[2:].zfill(8))
    return bin_representation

def parse_bit_mask(si, offset):
    bin_representation = si_to_bin(si)
    neighbours = []
    for x in xrange(1,125):
        index = 0-x
        bit = bin_representation[index]
        if bit == '1':
            neighbours.append(abs(index) + offset)
    return neighbours


def parse_900(si):
    neighbours = parse_bit_mask(si, 0)
    print neighbours

def parse_1800(si,siter,sibis):
    pass

def parse_900_ext(si):
    pass



system_information_1 =  '59 06 1A 0B B9 3B 30 00 00 00 00 00 00 00 00 00 00 00 00 80 9D 00 00'
system_information_2 =  '59 06 1a 00 00 00 00 02 10 00 00 00 00 00 48 20 95 00 00 08 a5 00 00'
system_information_3 =  '59 06 1a 00 00 00 0b 90 08 00 00 00 01 14 08 00 11 00 00 88 a5 00 00'
system_information_4 =  '59 06 1a 00 00 00 0b 90 08 00 00 00 01 14 08 00 11 00 00 88 a5 00 00'

parse_900(system_information_2)










