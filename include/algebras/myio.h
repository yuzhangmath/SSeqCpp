#ifndef MYIO_H
#define MYIO_H

#include <fmt/core.h>
#include <cstring>
#include <sstream>
#include <vector>

namespace myio {

using int1d = std::vector<int>;
using int2d = std::vector<int1d>;
using int3d = std::vector<int2d>;

template <typename FwdIt, typename FnStr>
std::string TplStrCont(const char* left, const char* sep, const char* right, const char* empty, FwdIt first, FwdIt last, FnStr str)  // TODO: make a performant version
{
    std::string result;
    if (first == last)
        result += empty;
    else {
        result += left;
        result += str(*first);
        while (++first != last) {
            result += sep;
            result += str(*first);
        }
        result += right;
    }
    return result;
}

template <typename Container, typename FnStr>
std::string StrCont(const char* left, const char* sep, const char* right, const char* empty, const Container& cont, FnStr str)  // TODO: make a performant version
{
    return TplStrCont(left, sep, right, empty, cont.begin(), cont.end(), str);
}

/* Consume `pattern` from `sin`. Set badbit if not found. */
void consume(std::istream& sin, const char* pattern);
inline std::istream& operator>>(std::istream& sin, const char* pattern)
{
    consume(sin, pattern);
    return sin;
}

/* Load container from an istream */
template <typename Container>
void load_vector(std::istream& sin, Container& cont, const char* left, const char* sep, const char* right)
{
    cont.clear();
    sin >> std::ws;
    consume(sin, left);
    typename Container::value_type ele;
    while (sin.good()) {
        sin >> ele;
        cont.push_back(ele);
        sin >> std::ws;
        consume(sin, sep);
    }
    if (sin.bad()) {
        sin.clear();
        consume(sin, right);
    }
}

/* Load the container from an istream */
template <typename Container, typename _Fn_load>
void load_vector(std::istream& sin, Container& cont, const char* left, const char* sep, const char* right, _Fn_load load)
{
    cont.clear();
    sin >> std::ws;
    consume(sin, left);
    typename Container::value_type ele;
    while (sin.good()) {
        load(sin, ele);
        cont.push_back(std::move(ele));
        sin >> std::ws;
        consume(sin, sep);
    }
    if (sin.bad()) {
        sin.clear();
        consume(sin, right);
    }
}

inline void load_array(std::istream& sin, int1d& a)
{
    load_vector(sin, a, "(", ",", ")");
}
inline void load_array2d(std::istream& sin, int2d& a)
{
    load_vector(sin, a, "[", ",", "]", load_array);
}
inline void load_array3d(std::istream& sin, int3d& a)
{
    load_vector(sin, a, "{", ",", "}", load_array2d);
}
inline std::istream& operator>>(std::istream& sin, int1d& a)
{
    load_array(sin, a);
    return sin;
}
inline std::istream& operator>>(std::istream& sin, int2d& a)
{
    load_array2d(sin, a);
    return sin;
}
inline std::istream& operator>>(std::istream& sin, int3d& a)
{
    load_array3d(sin, a);
    return sin;
}

template <typename T>
int load_op_arg(int argc, char** argv, int index, const char* name, T& x)
{
    if (index < argc) {
        if constexpr (std::is_same<T, std::string>::value) {
            if (strlen(argv[index]) == 0)
                return 0;
        }
        std::istringstream ss(argv[index]);
        if (!(ss >> x)) {
            fmt::print("Invalid: {}: {}\n", name, argv[index]);
            return 1;
        }
    }
    return 0;
}

template <typename T>
int load_arg(int argc, char** argv, int index, const char* name, T& x)
{
    if (index < argc) {
        return load_op_arg(argc, argv, index, name, x);
    }
    else {
        fmt::print("Mising argument <{}>\n", name);
        return 2;
    }
    return 0;
}

template <typename T>
int load_args(int argc, char** argv, int index, const char* name, std::vector<T>& x)
{
    if (index < argc) {
        x.resize(size_t(argc - index));
        for (size_t i = index; i < (int)argc; ++i) {
            if (int error = load_op_arg(argc, argv, (int)i, name, x[i - (size_t)index]))
                return error;
        }
    }
    return 0;
}

/* Return true if user inputs Y; Return false if user inputs N */
bool UserConfirm();
void AssertFileExists(const std::string& filename);

}  // namespace myio

#endif /* MYIO_H */