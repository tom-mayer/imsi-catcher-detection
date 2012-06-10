max_prime = 30#775146
candidates = range(2,max_prime)
current_prime = 5
primes = [2,3,5]

while candidates and current_prime < max_prime:
    if len(primes) %10 == 0:
        print current_prime
    for number in candidates:
        if number % current_prime == 0:
            candidates.remove(number)
    current_prime = candidates[0]
    candidates.remove(current_prime)
    primes.append(current_prime)

print primes

result = 0

for number in primes:
    if 600851475143%number ==0:
        result = number

print result
