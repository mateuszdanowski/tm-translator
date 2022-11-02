num-tapes: 2
input-alphabet: a b ((a))

# next, we have a list of transitions, in the format:
# <p> <a_1> ... <a_k> <q> <b_1> ... <b_k> <d_1> ... <d_k>
# where:
# * p and q are states before / after the transition
# * a_1, ..., a_k are letters under the head on tapes 1,...,k before the transition
# * b_1, ..., b_k - likewise, but after the transition
# * d_1, ..., d_k - directions in which each head is moved; can be <, >, -

(start) a _ (check) ((a)) b - -
(start) b _ (check) a a - -
(start) ((a)) _ (check) ((a)) a - -
(check) a b (accept) a b - -
(check) a a (accept) b a - -
(check) ((a)) a (accept) ((a)) a - -

(start) _ _ (accept) _ _ - -  # special case: the empty word should also be accepted


