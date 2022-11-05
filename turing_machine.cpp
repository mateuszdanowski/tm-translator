#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include "turing_machine.h"

using namespace std;

class Reader {
public:
    bool is_next_token_available() {
        return next_char != '\n' && next_char != EOF;
    }
    
    string next_token() { // only in the current line
        assert(is_next_token_available());
        string res;
        while (next_char != ' ' && next_char != '\t' && next_char != '\n' && next_char != EOF)
            res += get_next_char();
        skip_spaces();
        return res;
    }
    
    void go_to_next_line() { // in particular skips empty lines
        assert(!is_next_token_available());
        while (next_char == '\n') {
            get_next_char();
            skip_spaces();
        }
    }
    
    ~Reader() {
        assert(fclose(input) == 0);
    }
    
    Reader(FILE *input_) : input(input_) {
        assert(input);
        get_next_char();
        skip_spaces();
        if (!is_next_token_available())
            go_to_next_line();
    }
    
    int get_line_num() const {
        return line;
    }

private:
    FILE *input;
    int next_char; // we always have the next char here
    int line = 1;
    
    int get_next_char() {
        if (next_char == '\n')
            ++line;
        int prev = next_char;
        next_char = fgetc(input);
        if (next_char == '#') // skip a comment until EOL or EOF
            while (next_char != '\n' && next_char != EOF)
                next_char = fgetc(input);
        return prev;
    }
    
    void skip_spaces() {
        while (next_char == ' ' || next_char == '\t')
            get_next_char();
    }
};

static bool is_valid_char(int ch) {
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9')
        || ch == '_' || ch == '-';
}

static bool is_direction(int ch) {
    return ch == HEAD_LEFT || ch == HEAD_RIGHT || ch == HEAD_STAY;
}

// searches for an identifier starting from position pos;
// at the end pos is the position after the identifier
// (if false returned, pos remains unchanged)
static bool check_identifier(string ident, size_t &pos) {
    if (pos >= ident.size())
        return false;
    if (is_valid_char(ident[pos])) {
        ++pos;
        return true;
    }
    if (ident[pos] != '(')
        return false;
    size_t pos2 = pos + 1;
    while (check_identifier(ident, pos2));
    if (pos2 == pos + 1 || pos2 >= ident.size() || ident[pos2] != ')')
        return false;
    pos = pos2 + 1;
    return true;
}

static bool is_identifier(string ident) {
    size_t pos = 0;
    return check_identifier(ident, pos) && pos == ident.length();
}

TuringMachine::TuringMachine(int num_tapes_, vector<string> input_alphabet_, transitions_t transitions_)
    : num_tapes(num_tapes_), input_alphabet(input_alphabet_), transitions(transitions_) {
    assert(num_tapes > 0);
    assert(!input_alphabet.empty());
    for (auto letter : input_alphabet)
        assert(is_identifier(letter) && letter != BLANK);
    for (auto transition : transitions) {
        auto state_before = transition.first.first;
        auto letters_before = transition.first.second;
        auto state_after = get<0>(transition.second);
        auto letters_after = get<1>(transition.second);
        auto directions = get<2>(transition.second);
        assert(is_identifier(state_before) && state_before != ACCEPTING_STATE && state_before != REJECTING_STATE && is_identifier(state_after));
        assert(letters_before.size() == (size_t)num_tapes && letters_after.size() == (size_t)num_tapes && directions.length() == (size_t)num_tapes);
        for (int a = 0; a < num_tapes; ++a)
            assert(is_identifier(letters_before[a]) && is_identifier(letters_after[a]) && is_direction(directions[a]));
    }
}

#define syntax_error(reader, message) \
    for(;;) { \
        cerr << "Syntax error in line " << reader.get_line_num() << ": " << message << "\n"; \
        exit(1); \
    }

static string read_identifier(Reader &reader) {
    if (!reader.is_next_token_available())
        syntax_error(reader, "Identifier expected");
    string ident = reader.next_token();
    size_t pos = 0;
    if (!check_identifier(ident, pos) || pos != ident.length())
        syntax_error(reader, "Invalid identifier \"" << ident << "\"");
    return ident;
}

#define NUM_TAPES "num-tapes:"
#define INPUT_ALPHABET "input-alphabet:"

TuringMachine read_tm_from_file(FILE *input) {
    Reader reader(input);

    // number of tapes
    int num_tapes;
    if (!reader.is_next_token_available() || reader.next_token() != NUM_TAPES)
        syntax_error(reader, "\"" NUM_TAPES "\" expected");
    try {
        if (!reader.is_next_token_available())
            throw 0;
        string num_tapes_str = reader.next_token();
        size_t last;
        num_tapes = stoi(num_tapes_str, &last);
        if (last != num_tapes_str.length() || num_tapes <= 0)
            throw 0;
    } catch (...) {
        syntax_error(reader, "Positive integer expected after \"" NUM_TAPES "\"");
    }
    if (reader.is_next_token_available())
        syntax_error(reader, "Too many tokens in a line");
    reader.go_to_next_line();
    
    // input alphabet
    vector<string> input_alphabet;
    if (!reader.is_next_token_available() || reader.next_token() != INPUT_ALPHABET)
        syntax_error(reader, "\"" INPUT_ALPHABET "\" expected");
    while (reader.is_next_token_available()) {
        input_alphabet.emplace_back(read_identifier(reader));
        if (input_alphabet.back() == BLANK)
            syntax_error(reader, "The blank letter \"" BLANK "\" is not allowed in the input alphabet");
    }
    if (input_alphabet.empty())
        syntax_error(reader, "Identifier expected");
    reader.go_to_next_line();
    
    // transitions
    transitions_t transitions;
    while (reader.is_next_token_available()) {
        string state_before = read_identifier(reader);
        if (state_before == "(accept)" || state_before == "(reject)")
            syntax_error(reader, "No transition can start in the \"" << state_before << "\" state");

        vector<string> letters_before;
        for (int a = 0; a < num_tapes; ++a)
            letters_before.emplace_back(read_identifier(reader));

        if (transitions.find(make_pair(state_before, letters_before)) != transitions.end())
            syntax_error(reader, "The machine is not deterministic");

        string state_after = read_identifier(reader);

        vector<string> letters_after;
        for (int a = 0; a < num_tapes; ++a)
            letters_after.emplace_back(read_identifier(reader));

        string directions;
        for (int a = 0; a < num_tapes; ++a) {
            string dir;
            if (!reader.is_next_token_available() || (dir = reader.next_token()).length() != 1 || !is_direction(dir[0]))
                syntax_error(reader, "Move direction expected, which should be " << HEAD_LEFT << ", " << HEAD_RIGHT << ", or " << HEAD_STAY);
            directions += dir;
        }
        
        if (reader.is_next_token_available()) 
            syntax_error(reader, "Too many tokens in a line");
        reader.go_to_next_line();
        
        transitions[make_pair(state_before, letters_before)] = make_tuple(state_after, letters_after, directions);
    }
    
    return TuringMachine(num_tapes, input_alphabet, transitions);
}

vector<string> TuringMachine::working_alphabet() const {
    set<string> letters(input_alphabet.begin(), input_alphabet.end());
    letters.insert(BLANK);
    for (auto transition : transitions) {
        auto letters_before = transition.first.second;
        auto letters_after = get<1>(transition.second);
        letters.insert(letters_before.begin(), letters_before.end());
        letters.insert(letters_after.begin(), letters_after.end());
    }
    return vector<string>(letters.begin(), letters.end());
}
    
vector<string> TuringMachine::set_of_states() const {
    set<string> states;
    states.insert(INITIAL_STATE);
    states.insert(ACCEPTING_STATE);
    states.insert(REJECTING_STATE);
    for (auto transition : transitions) {
        states.insert(transition.first.first);
        states.insert(get<0>(transition.second));
    }
    return vector<string>(states.begin(), states.end());
}

static void output_vector(ostream &output, vector<string> v) {
   for (string el : v)
        output << " " << el;
}
    
void TuringMachine::save_to_file(ostream &output) const {
    output << NUM_TAPES << " " << num_tapes << "\n"
           << INPUT_ALPHABET;
    output_vector(output, input_alphabet);
    output << "\n";
    for (auto transition : transitions) {
        output << transition.first.first;
        output_vector(output, transition.first.second);
        output << " " << get<0>(transition.second);
        output_vector(output, get<1>(transition.second));
        string directions = get<2>(transition.second);
        for (int a = 0; a < num_tapes; ++a)
            output << " " << directions[a];
        output << "\n";
    }
}

vector<string> TuringMachine::parse_input(std::string input) const {
    set<string> alphabet(input_alphabet.begin(), input_alphabet.end());
    size_t pos = 0;
    vector<string> res;
    while (pos < input.length()) {
        size_t prev_pos = pos;
        if (!check_identifier(input, pos))
            return vector<string>();
        res.emplace_back(input.substr(prev_pos, pos - prev_pos));
        if (alphabet.find(res.back()) == alphabet.end())
            return vector<string>();
    }
    return res;
}

/** TRANSLATOR */

// The mapping for input alphabet (letter -> letter with head)
typedef std::map<std::string, std::string> IdentifiersMapping;

transitions_t create_init_transitions(const TuringMachine &tm, const IdentifiersMapping &mapping,
                                      const string &SEPARATOR, const string &TAPE_END) {
    const string INIT_FIRST_TAPE_STATE = "(init_1st_tape)";
    const string INIT_SECOND_TAPE_STATE = "(init_2nd_tape)";
    const string INIT_PUT_TAPE_END_STATE = "(init_put_tape_end)";
    const string INIT_GO_TO_SEPARATOR = "(init_go_to_separator)";
    const string INIT_GO_TO_BEGINNING_STATE = "(init_go_to_beginning)";
    const string START_SEARCH_FIRST_HEAD_STATE = "((start)-(search_1st_head))";

    transitions_t transitions;

    // start off for the blank
    transitions[make_pair(INITIAL_STATE, vector<string>{BLANK})] = make_tuple(INIT_FIRST_TAPE_STATE, vector<string>{mapping.at(BLANK)}, ">");
    for (const auto &letter: tm.input_alphabet) {
        // start off for any letter
        transitions[make_pair(INITIAL_STATE, vector<string>{letter})] = make_tuple(INIT_FIRST_TAPE_STATE, vector<string>{mapping.at(letter)}, ">");

        // go to the end of the first tape, until the blank is found
        transitions[make_pair(INIT_FIRST_TAPE_STATE, vector<string>{letter})] = make_tuple(INIT_FIRST_TAPE_STATE, vector<string>{letter}, ">");

        // after the initialization, go to the beginning of the tape
        transitions[make_pair(INIT_GO_TO_BEGINNING_STATE, vector<string>{letter})] = make_tuple(INIT_GO_TO_BEGINNING_STATE, vector<string>{letter}, "<");

        // stop at the beginning, that is where the head is located, and go to the starting state of translated Turing Machine
        transitions[make_pair(INIT_GO_TO_BEGINNING_STATE, vector<string>{mapping.at(letter)})] = make_tuple(START_SEARCH_FIRST_HEAD_STATE, vector<string>{mapping.at(letter)}, "-");
    }
    // at the beginning there could also be a blank with head
    transitions[make_pair(INIT_GO_TO_BEGINNING_STATE, vector<string>{mapping.at(BLANK)})] = make_tuple(START_SEARCH_FIRST_HEAD_STATE, vector<string>{mapping.at(BLANK)}, "-");


    // when the blank is found, put there a separator
    transitions[make_pair(INIT_FIRST_TAPE_STATE, vector<string>{BLANK})] = make_tuple(INIT_SECOND_TAPE_STATE, vector<string>{SEPARATOR}, ">");

    // put a blank with a head after the separator
    transitions[make_pair(INIT_SECOND_TAPE_STATE, vector<string>{BLANK})] = make_tuple(INIT_PUT_TAPE_END_STATE, vector<string>{mapping.at(BLANK)}, ">");

    // at the end, put the end of tape identifier
    transitions[make_pair(INIT_PUT_TAPE_END_STATE, vector<string>{BLANK})] = make_tuple(INIT_GO_TO_SEPARATOR, vector<string>{TAPE_END}, "<");

    // go one left, where the separator should be
    transitions[make_pair(INIT_GO_TO_SEPARATOR, vector<string>{mapping.at(BLANK)})] = make_tuple(INIT_GO_TO_SEPARATOR, vector<string>{mapping.at(BLANK)}, "<");

    // when the separator is found, go left to the beginning
    transitions[make_pair(INIT_GO_TO_SEPARATOR, vector<string>{SEPARATOR})] = make_tuple(INIT_GO_TO_BEGINNING_STATE, vector<string>{SEPARATOR}, "<");

    return transitions;
}

void translate_state_transitions(transitions_t &transitions, const string &state,
                                 const TuringMachine &tm, const IdentifiersMapping &mapping,
                                 const string &SEPARATOR, const string &TAPE_END) {
    const string SEARCH_1ST_HEAD_STATE = "(" + state + "-(search_1st_head))";

    /** Search 1st head (go left) */
    // when separator is found, go left
    transitions[make_pair(SEARCH_1ST_HEAD_STATE, vector<string>{SEPARATOR})] = make_tuple(SEARCH_1ST_HEAD_STATE, vector<string>{SEPARATOR}, "<");
    for (const auto &letterA: tm.working_alphabet()) {
        // when a letter without a head is found, go left
        transitions[make_pair(SEARCH_1ST_HEAD_STATE, vector<string>{letterA})] = make_tuple(SEARCH_1ST_HEAD_STATE, vector<string>{letterA}, "<");

        const string SEARCH_2ND_HEAD_STATE = "(" + state + "-(" + letterA + ")-(search_2nd_head))";
        // when a letter WITH a head is found, store it in the state and start going right
        transitions[make_pair(SEARCH_1ST_HEAD_STATE, vector<string>{mapping.at(letterA)})] = make_tuple(SEARCH_2ND_HEAD_STATE, vector<string>{mapping.at(letterA)}, ">");

        /** Search 2nd head (go right) */
        transitions[make_pair(SEARCH_2ND_HEAD_STATE, vector<string>{SEPARATOR})] = make_tuple(SEARCH_2ND_HEAD_STATE, vector<string>{SEPARATOR}, ">");
        for (const auto &letterB: tm.working_alphabet()) {
            // when a letter without a head is found, go right
            transitions[make_pair(SEARCH_2ND_HEAD_STATE, vector<string>{letterB})] = make_tuple(SEARCH_2ND_HEAD_STATE, vector<string>{letterB}, ">");

            const string GO_1ST_HEAD_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(go_1st_head))";
            // when a letter WITH a head is found, store it in the state and start going left
            transitions[make_pair(SEARCH_2ND_HEAD_STATE, vector<string>{mapping.at(letterB)})] = make_tuple(GO_1ST_HEAD_STATE, vector<string>{mapping.at(letterB)}, "<");

            // if there is no transition from <(state), letterA, letterB>, then there is no need to create any more transitions
            if (tm.transitions.find(make_pair(state, vector<string>{letterA, letterB})) == tm.transitions.end()) {
                continue;
            }
            // else, prepare for creating the next transition
            auto next_move = tm.transitions.at(make_pair(state, vector<string>{letterA, letterB}));
            string next_state = get<0>(next_move);

            vector<string> next_letters = get<1>(next_move);
            string head_moves = get<2>(next_move);

            const char tape_1st_head_move = head_moves[0];
            const char tape_2nd_head_move = head_moves[1];
            const string tape_1st_next_letter = next_letters[0];
            const string tape_2nd_next_letter = next_letters[1];
            string NEXT_STATE = "(" + next_state + "-(search_1st_head))";
            if (next_state == ACCEPTING_STATE) {
                NEXT_STATE = ACCEPTING_STATE;
            }
            if (next_state == REJECTING_STATE) {
                NEXT_STATE = REJECTING_STATE;
            }

            // when separator is found, go left
            transitions[make_pair(GO_1ST_HEAD_STATE, vector<string>{SEPARATOR})] = make_tuple(GO_1ST_HEAD_STATE, vector<string>{SEPARATOR}, "<");

            // prepare for finding the 2nd head (creating similar states)
            const string GO_2ND_HEAD_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(go_2nd_head))";
            transitions[make_pair(GO_2ND_HEAD_STATE, vector<string>{SEPARATOR})] = make_tuple(GO_2ND_HEAD_STATE, vector<string>{SEPARATOR}, ">");

            for (const auto &letter: tm.working_alphabet()) {
                // when a letter without a head is found, continue going left/right
                transitions[make_pair(GO_1ST_HEAD_STATE, vector<string>{letter})] = make_tuple(GO_1ST_HEAD_STATE, vector<string>{letter}, "<");
                transitions[make_pair(GO_2ND_HEAD_STATE, vector<string>{letter})] = make_tuple(GO_2ND_HEAD_STATE, vector<string>{letter}, ">");

                // when the 1st head is found, do the operation (move head and put new letter)
                const string PUT_1ST_HEAD_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(put_1st_head))";
                if (tape_1st_head_move == '<') {
                    transitions[make_pair(GO_1ST_HEAD_STATE, vector<string>{mapping.at(letterA)})] = make_tuple(PUT_1ST_HEAD_STATE, vector<string>{tape_1st_next_letter}, "<");
                    for (const auto &any_letter: tm.working_alphabet()) {
                        transitions[make_pair(PUT_1ST_HEAD_STATE, vector<string>{any_letter})] = make_tuple(GO_2ND_HEAD_STATE, vector<string>{mapping.at(any_letter)}, ">");
                    }
                }
                if (tape_1st_head_move == '-') {
                    transitions[make_pair(GO_1ST_HEAD_STATE, vector<string>{mapping.at(letterA)})] = make_tuple(GO_2ND_HEAD_STATE, vector<string>{mapping.at(tape_1st_next_letter)}, ">");
                }
                // if the next head move is right, then we need to check if there is space
                if (tape_1st_head_move == '>') {
                    const string PUT_1ST_HEAD_WITH_CHECK_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(put_1st_head_with_check))";

                    transitions[make_pair(GO_1ST_HEAD_STATE, vector<string>{mapping.at(letterA)})] = make_tuple(PUT_1ST_HEAD_WITH_CHECK_STATE, vector<string>{tape_1st_next_letter}, ">");

                    // no separator found, put the head there
                    for (const auto &any_letter: tm.working_alphabet()) {
                        transitions[make_pair(PUT_1ST_HEAD_WITH_CHECK_STATE, vector<string>{any_letter})] = make_tuple(GO_2ND_HEAD_STATE, vector<string>{mapping.at(any_letter)}, ">");
                    }

                    // if we find a separator where we want to move the head, then we must shift everything one to the right
                    const string SHIFT_ALL_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(shift_all))";
                    transitions[make_pair(PUT_1ST_HEAD_WITH_CHECK_STATE, vector<string>{SEPARATOR})] = make_tuple(SHIFT_ALL_STATE, vector<string>{SEPARATOR}, ">");

                    // go right until we find the end-tape char
                    for (const auto &any_letter_shift: tm.working_alphabet()) {
                        transitions[make_pair(SHIFT_ALL_STATE, vector<string>{any_letter_shift})] = make_tuple(SHIFT_ALL_STATE, vector<string>{any_letter_shift}, ">");
                        transitions[make_pair(SHIFT_ALL_STATE, vector<string>{mapping.at(any_letter_shift)})] = make_tuple(SHIFT_ALL_STATE, vector<string>{mapping.at(any_letter_shift)}, ">");
                    }

                    const string SHIFT_EACH_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(shift_each))";
                    const string SHIFT_END_TAPE_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(shift_end_tape))";
                    const string GO_ONE_LEFT_INIT_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(go_one_left_init_state))";

                    // tape-end found, now we have to shift each cell until we find a separator
                    transitions[make_pair(SHIFT_ALL_STATE, vector<string>{TAPE_END})] = make_tuple(SHIFT_END_TAPE_STATE, vector<string>{BLANK}, ">");
                    transitions[make_pair(SHIFT_END_TAPE_STATE, vector<string>{BLANK})] = make_tuple(GO_ONE_LEFT_INIT_STATE, vector<string>{TAPE_END}, "<");

                    transitions[make_pair(GO_ONE_LEFT_INIT_STATE, vector<string>{BLANK})] = make_tuple(SHIFT_EACH_STATE, vector<string>{BLANK}, "<");

                    for (const auto &any_letter: tm.working_alphabet()) {
                        const string SHIFT_PUT_STATE1 = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(" + any_letter + ")-(shift_put_state1))";
                        const string GO_ONE_LEFT_STATE1 = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(" + any_letter + ")-(go_one_left_state1))";
                        transitions[make_pair(SHIFT_EACH_STATE, vector<string>{any_letter})] = make_tuple(SHIFT_PUT_STATE1, vector<string>{BLANK}, ">");
                        transitions[make_pair(SHIFT_PUT_STATE1, vector<string>{BLANK})] = make_tuple(GO_ONE_LEFT_STATE1, vector<string>{any_letter}, "<");
                        transitions[make_pair(GO_ONE_LEFT_STATE1, vector<string>{BLANK})] = make_tuple(SHIFT_EACH_STATE, vector<string>{BLANK}, "<");

                        const auto &mapped_letter = mapping.at(any_letter);
                        const string SHIFT_PUT_STATE2 = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(" + mapped_letter + ")-(shift_put_state2))";
                        const string GO_ONE_LEFT_STATE2 = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(" + mapped_letter + ")-(go_one_left_state2))";
                        transitions[make_pair(SHIFT_EACH_STATE, vector<string>{mapped_letter})] = make_tuple(SHIFT_PUT_STATE2, vector<string>{BLANK}, ">");
                        transitions[make_pair(SHIFT_PUT_STATE2, vector<string>{BLANK})] = make_tuple(GO_ONE_LEFT_STATE2, vector<string>{mapped_letter}, "<");
                        transitions[make_pair(GO_ONE_LEFT_STATE2, vector<string>{BLANK})] = make_tuple(SHIFT_EACH_STATE, vector<string>{BLANK}, "<");
                    }
                    const string SHIFT_PUT_SEPARATOR_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(" + SEPARATOR + ")-(shift_put_separator_state))";
                    const string GO_ONE_LEFT_SEPARATOR_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(" + SEPARATOR + ")-(go_one_left_separator_state))";

                    // all shifted, separator found
                    transitions[make_pair(SHIFT_EACH_STATE, vector<string>{SEPARATOR})] = make_tuple(SHIFT_PUT_SEPARATOR_STATE, vector<string>{BLANK}, ">");
                    transitions[make_pair(SHIFT_PUT_SEPARATOR_STATE, vector<string>{BLANK})] = make_tuple(GO_ONE_LEFT_SEPARATOR_STATE, vector<string>{SEPARATOR}, "<");
                    transitions[make_pair(GO_ONE_LEFT_SEPARATOR_STATE, vector<string>{BLANK})] = make_tuple(GO_2ND_HEAD_STATE, vector<string>{mapping.at(BLANK)}, ">");
                }

                // when the 2nd head is found, do the operation (put new letter and move head)
                const string PUT_2ND_HEAD_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(put_2nd_head))";
                if (tape_2nd_head_move == '<') {
                    transitions[make_pair(GO_2ND_HEAD_STATE, vector<string>{mapping.at(letterB)})] = make_tuple(PUT_2ND_HEAD_STATE, vector<string>{tape_2nd_next_letter}, "<");
                    for (const auto &any_letter: tm.working_alphabet()) {
                        transitions[make_pair(PUT_2ND_HEAD_STATE, vector<string>{any_letter})] = make_tuple(NEXT_STATE, vector<string>{mapping.at(any_letter)}, "<");
                    }
                }
                if (tape_2nd_head_move == '-') {
                    transitions[make_pair(GO_2ND_HEAD_STATE, vector<string>{mapping.at(letterB)})] = make_tuple(NEXT_STATE, vector<string>{mapping.at(tape_2nd_next_letter)}, "<");
                }
                // if the next head move is right, then we need to check if there is no tape-end
                if (tape_2nd_head_move == '>') {
                    const string PUT_2ND_HEAD_WITH_CHECK_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(put_2nd_head_with_check))";
                    const string PUT_TAPE_END_AFTER_2ND_HEAD_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(put_tape_end_after_2nd_head))";
                    const string GO_BACK_AFTER_PUTTING_TAPE_END_STATE = "(" + state + "-(" + letterA + ")-(" + letterB + ")-(go_back_after_putting_tape_end))";

                    // 2nd head found, check if there is space to the right
                    transitions[make_pair(GO_2ND_HEAD_STATE, vector<string>{mapping.at(letterB)})] = make_tuple(PUT_2ND_HEAD_WITH_CHECK_STATE, vector<string>{tape_2nd_next_letter}, ">");

                    // there is no tape-end
                    for (const auto &any_letter: tm.working_alphabet()) {
                        transitions[make_pair(PUT_2ND_HEAD_WITH_CHECK_STATE, vector<string>{any_letter})] = make_tuple(NEXT_STATE, vector<string>{mapping.at(any_letter)}, "<");
                    }

                    // there is a tape-end
                    transitions[make_pair(PUT_2ND_HEAD_WITH_CHECK_STATE, vector<string>{TAPE_END})] = make_tuple(PUT_TAPE_END_AFTER_2ND_HEAD_STATE, vector<string>{mapping.at(BLANK)}, ">");
                    transitions[make_pair(PUT_TAPE_END_AFTER_2ND_HEAD_STATE, vector<string>{BLANK})] = make_tuple(GO_BACK_AFTER_PUTTING_TAPE_END_STATE, vector<string>{TAPE_END}, "<");
                    transitions[make_pair(GO_BACK_AFTER_PUTTING_TAPE_END_STATE, vector<string>{mapping.at(BLANK)})] = make_tuple(NEXT_STATE, vector<string>{mapping.at(BLANK)}, "<");
                }
            }
        }
    }
}

transitions_t translate_transitions(const TuringMachine &tm, const IdentifiersMapping &mapping,
                                    const string &SEPARATOR, const string &TAPE_END) {
    transitions_t transitions;
    auto set_of_states = tm.set_of_states();
    for (const auto &state: set_of_states) {
        translate_state_transitions(transitions, state, tm, mapping, SEPARATOR, TAPE_END);
    }
    return transitions;
}

int calc_max_depth(const string &s) {
    int depth = 0;
    int max_depth = 0;
    for (auto c: s) {
        if (c == '(') {
            depth++;
        }
        if (c == ')') {
            depth--;
        }
        max_depth = max(max_depth, depth);
    }
    return max_depth;
}

int calc_max_depth_foreach(const vector<string> &v) {
    int max_depth = 0;
    for (const auto &elem: v) {
        max_depth = max(max_depth, calc_max_depth(elem));
    }
    return max_depth;
}

string wrap_with_parentheses(const string &s, int count) {
    string result;
    for (int i = 0; i < count; i++) {
        result += '(';
    }
    result += s;
    for (int i = 0; i < count; i++) {
        result += ')';
    }
    return result;
}

IdentifiersMapping map_letters_from_alphabet(const vector<string> &alphabet, int parentheses_to_add) {
    IdentifiersMapping mapping;
    for (const auto &identifier: alphabet) {
        mapping[identifier] = wrap_with_parentheses(identifier, parentheses_to_add);
    }
    mapping[BLANK] = wrap_with_parentheses(BLANK, parentheses_to_add);
    return mapping;
}

TuringMachine translate_tm(const TuringMachine &tm) {
    int parentheses_to_add = calc_max_depth_foreach(tm.working_alphabet()) + 1;

    IdentifiersMapping mapping = map_letters_from_alphabet(tm.working_alphabet(), parentheses_to_add);
    const string SEPARATOR = wrap_with_parentheses("(separator)", parentheses_to_add + 1);
    const string TAPE_END = wrap_with_parentheses("(tape-end)", parentheses_to_add + 1);

    auto init_transitions = create_init_transitions(tm, mapping, SEPARATOR, TAPE_END);
    auto translated_transitions = translate_transitions(tm, mapping, SEPARATOR, TAPE_END);

    translated_transitions.insert(init_transitions.begin(), init_transitions.end());
    TuringMachine one_tape_tm = TuringMachine(1, tm.input_alphabet, translated_transitions);
    return one_tape_tm;
}







