#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

using namespace std;

template <typename T> void print(T s) {
    for (auto c : s) {
        cout << c;
    }
    cout << endl;
}

template <class T> class Span {
  public:
    Span(const T *const buf, size_t N) : buf_(buf), N_(N) {}

    typedef T *iterator;
    const T *begin() const { return buf_; }
    const T *end() const { return buf_ + N_; }
    size_t size() const { return N_; }
    bool empty() const { return !N_; }
    const T &operator[](size_t i) const { return buf_[i]; }

    Span<T> slice(size_t start, size_t end) { return Span<T>(buf_ + start, end - start); }

  private:
    const T *const buf_;
    const size_t N_;
};

class BaseMatcher {
  public:
    virtual ~BaseMatcher() = default;
    virtual vector<Span<char>> chomp(Span<char>) const = 0;

    virtual ostream &operator<<(ostream &os) {
        os << this->_name << "(" << _c << ")";
        return os;
    }

  protected:
    BaseMatcher(char c, string &&name) : _c(c), _name(move(name)) {}
    const char _c;
    const string _name;
};

class SingleMatcher : public BaseMatcher {
  public:
    SingleMatcher(char c) : BaseMatcher(c, "SingleMatcher") {}

    vector<Span<char>> chomp(Span<char> s) const override {
        if (s.size() == 0 || s[0] != _c) {
            return {};
        }

        return {s.slice(1, s.size())};
    }
};

class PlusMatcher : public BaseMatcher {
  public:
    PlusMatcher(char c) : BaseMatcher(c, "PlusMatcher") {}
    vector<Span<char>> chomp(Span<char> s) const override {
        vector<Span<char>> v;
        if (s.size() == 0) {
            return {};
        }

        for (auto i = 0; i < s.size() && s[i] == _c; ++i) {
            v.push_back(s.slice(i + 1, s.size()));
        }

        return v;
    }
};

class StarMatcher : public BaseMatcher {
  public:
    StarMatcher(char c) : BaseMatcher(c, "StarMatcher") {}
    vector<Span<char>> chomp(Span<char> s) const override {
        vector<Span<char>> v;
        v.push_back(s);

        for (auto i = 0; i < s.size() && s[i] == _c; ++i) {
            v.push_back(s.slice(i + 1, s.size()));
        }

        return v;
    }
};

void print(Span<char> s) {
    for (auto c : s) {
        cout << c;
    }
    cout << endl;
}

class RangeMatcher : public BaseMatcher {
  public:
    RangeMatcher(char c, size_t n1, size_t n2) : BaseMatcher(c, "RangeMatcher"), low_(n1), high_(n2) {
        if (high_ == 0 || low_ > high_) {
            throw exception();
        }
    }

    vector<Span<char>> chomp(Span<char> s) const override {
        vector<Span<char>> v;

        auto low = min(low_, s.size());
        auto high = min(high_, s.size());

        for (auto i = 0; i < low_; ++i) {
            if (s[i] != _c) {
                return v;
            }
        }

        if (low == s.size()) {
            if (low == low_) {
                v.push_back(s.slice(low, s.size()));
            }
            return v;
        }

        for (auto i = low; i <= high && s[i - 1] == _c; ++i) {
            v.push_back(s.slice(i, s.size()));
        }

        return v;
    }

  private:
    const size_t low_;
    const size_t high_;
};

struct range {
    int n1, n2;
};

struct range parse_range(string &&r) {
    // Super hacky, but fun to write
    range result;
    auto idx = r.find(',');
    r[idx] = '\0';

    result.n1 = atoi(r.data());

    auto c = r[r.size() - 1];
    r[r.size() - 1] = '\0';

    result.n2 = 10 * atoi(r.data() + idx + 1) + (c - '0');

    return result;
}

unique_ptr<BaseMatcher> parse_regex(char c, string &&regex) {
    if (regex == "") {
        return make_unique<SingleMatcher>(c);
    }

    if (regex == "*") {
        return make_unique<StarMatcher>(c);
    }

    if (regex == "+") {
        return make_unique<PlusMatcher>(c);
    }

    // num,num
    auto r = parse_range(move(regex));
    return make_unique<RangeMatcher>(c, r.n1, r.n2);
}

// parse attempts to convert a regex into its constituent matchers
vector<unique_ptr<BaseMatcher>> parse(const string &regex) {
    vector<unique_ptr<BaseMatcher>> v;
    if (regex.size() == 0) {
        return v;
    }

    // Get indexes of characters
    vector<size_t> indexes;
    indexes.reserve(regex.size());
    for (int i = 0; i < regex.size(); ++i) {
        auto c = regex[i];
        if (c >= 'a' && c <= 'z') {
            indexes.push_back(i);
        }
    }

    // The strings between characters are the regexes to parse
    for (int i = 0; i < indexes.size(); ++i) {
        auto c = regex[indexes[i]];
        auto low = indexes[i] + 1;
        auto high = (i < indexes.size() - 1) ? indexes[i + 1] : regex.size();

        auto matcher = parse_regex(c, regex.substr(low, high - low));
        v.push_back(move(matcher));
    }

    return v;
}

// _match recursively attempts to match s with the regex
// _match tries all matches greedily. If any one matches, it returns true
bool _match(const vector<unique_ptr<BaseMatcher>> &matchers, const Span<char> s, size_t regex_start) {
    if (regex_start == matchers.size()) {
        return s.empty();
    }

    auto suffixes = matchers[regex_start]->chomp(s);

    for (auto suffix : suffixes) {
        if (_match(matchers, suffix, regex_start + 1)) {
            return true;
        }
    }

    return false;
}

// matches returns whether the string s matches the regex regex.
// Currently supports the operators *, +, and a range n1,n2
bool matches(const string &s, const string &regex) {
    auto matchers = parse(regex);
    for (const auto &m : matchers) {
        cout << m.get() << ", ";
    }
    return _match(matchers, Span<char>(s.data(), s.size()), 0);
}

int main() {
    string s, regex;
    cout << "Enter a string and a regex to check for a match" << endl;
    cin >> s;
    cin >> regex;

    cout << "s: " << s << endl;
    cout << "regex: " << regex << endl;

    auto result = matches(s, regex) ? "true" : "false";
    cout << result << endl;
}
