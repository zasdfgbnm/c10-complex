#include <complex>
#include <iostream>

#if defined(__CUDACC__) || defined(__HIPCC__)
#include <thrust/complex.h>
#endif

namespace c10 {

using Half = short;  // Just for the convenience of prototyping
#if defined(__CUDACC__) || defined(__HIPCC__)
#define C10_HOST_DEVICE __host__ __device__ // Just for the convenience of prototyping
#else
#define C10_HOST_DEVICE
#endif

// c10::complex is an implementation of complex numbers that aims
// to work on all devices supported by PyTorch
//
// Most of the APIs duplicates std::complex
// Reference: https://en.cppreference.com/w/cpp/numeric/complex
//
// [Note on Constructors]
//
// The APIs of constructors are mostly copied from C++ standard:
//   https://en.cppreference.com/w/cpp/numeric/complex/complex
//
// Since C++14, all constructors are constexpr in std::complex
//
// There are three types of constructors:
// - initializing from real and imag: 
//     `constexpr complex( const T& re = T(), const T& im = T() );`
// - implicitly-declared copy constructor
// - converting constructors
//
// Converting constructors: 
// - std::complex defines converting constructor between float/double/long double,
//   while we define converting constructor between c10::Half/float/double.
// - For these converting constructors, upcasting is implicit, downcasting is
//   explicit.
// - We also define explicit casting from std::complex/thrust::complex
//   - Note that the conversion from thrust is not constexpr, because
//     thrust does not define them as constexpr ????
//
//
// [Operator =]
//
// The APIs of operator = are mostly copied from C++ standard:
//   https://en.cppreference.com/w/cpp/numeric/complex/operator%3D
//
// Since C++20, all operator= are constexpr. Although we are not bulding with
// C++20, we also obey this behavior.
//
// There are three types of assign operator:
// - Assign a real value from the same scalar type
//   - In std, this is templated as complex& operator=(const T& x)
//     with specialization `complex& operator=(T x)` for float/double/long double
//     Since we only support c10::Half, float and double, on will use `complex& operator=(T x)`
// - Copy assignment operator and converting assignment operator
//   - There is no specialization of converting assignemnt operators, which type is
//     convertible is soly depend on whether the scalar type is convertable
//
// In addition to the standard assignment, we also provide assignment operators with std and thrust
//
//
// [Casting operators]
//
// std::complex does not have casting operators. We define casting operators casting to std::complex and thrust::complex
//
//
// [Operator ""]
//
// std::complex has custom literals `i`, `if` and `if` defined in namespace `std::literals::complex_literals`.
// We define our own custom literals in the namespace `c10::complex_literals`. Our custom literals does not
// follow the same behavior as in std::complex, instead, we define _ih, _if, _id to construct half/float/double
// complex literals.
//
//
// [real() and imag()]
//
// In C++20, there are two overload of these functions, one it to return the real/imag, another is to set real/imag,
// they are both constexpr. We follow this design.
//
//
// [Operator +=,-=,*=,/=]
//
// Since C++20, these operators become constexpr. In our implementation, they are also constexpr.
//
// There are two types of such operators: operating with a real number, or operating with another complex number.
// For the operating with a real number, the generic template form has argument type `const T &`, while the overload
// for float/double/long double has `T`. We will follow the same type as float/double/long double in std.
//
// [Unary operator +-]
//
// Since C++20, they are constexpr. We also make them expr
//
// [Binary operators +-*/]
//
// Each operator has three versions (taking + as example):
// - complex + complex
// - complex + real
// - real + complex
//
// [Operator ==, !=]
// 
// Each operator has three versions (taking == as example):
// - complex == complex
// - complex == real
// - real == complex
// 
// Some of them are removed on C++20, but we decide to keep them
//
// [Operator <<, >>]
//
// These are implemented by casting to std::complex

template<typename T>
struct complex;

template<typename T>
struct alignas(sizeof(T) * 2) complex_common {
  T storage[2];

  constexpr complex_common(): storage{T(), T()} {}
  constexpr complex_common(const T& re, const T& im = T()): storage{re, im} {}
  template<typename U>
  constexpr complex_common(const std::complex<U> &other): complex_common(other.real(), other.imag()) {}
#if defined(__CUDACC__) || defined(__HIPCC__)
  template<typename U>
  C10_HOST_DEVICE complex_common(const thrust::complex<U> &other): complex_common(other.real(), other.imag()) {}
#endif

  constexpr complex<T> &operator =(T re) {
    storage[0] = re;
    return static_cast<complex<T> &>(*this);
  }

  constexpr complex<T> &operator +=(T re) {
    storage[0] += re;
    return static_cast<complex<T> &>(*this);
  }

  constexpr complex<T> &operator -=(T re) {
    storage[0] -= re;
    return static_cast<complex<T> &>(*this);
  }

  constexpr complex<T> &operator *=(T re) {
    storage[0] *= re;
    storage[1] *= re;
    return static_cast<complex<T> &>(*this);
  }

  constexpr complex<T> &operator /=(T re) {
    storage[0] /= re;
    storage[1] /= re;
    return static_cast<complex<T> &>(*this);
  }

  template<typename U>
  constexpr complex<T> &operator =(const complex<U> &rhs) {
    storage[0] = rhs.real();
    storage[1] = rhs.imag();
    return static_cast<complex<T> &>(*this);
  }

  template<typename U>
  constexpr complex<T> &operator +=(const complex<U> &rhs) {
    storage[0] += rhs.real();
    storage[1] += rhs.imag();
    return static_cast<complex<T> &>(*this);
  }

  template<typename U>
  constexpr complex<T> &operator -=(const complex<U> &rhs) {
    storage[0] -= rhs.real();
    storage[1] -= rhs.imag();
    return static_cast<complex<T> &>(*this);
  }

  template<typename U>
  constexpr complex<T> &operator *=(const complex<U> &rhs) {
    // (a + bi) * (c + di) = (a*c - b*d) + (a * d + b * c) i
    T a = storage[0];
    T b = storage[1];
    U c = rhs.real();
    U d = rhs.imag();
    storage[0] = a * c - b * d;
    storage[1] = a * d + b * c;
    return static_cast<complex<T> &>(*this);
  }

  template<typename U>
  constexpr complex<T> &operator /=(const complex<U> &rhs) {
    // (a + bi) / (c + di) = (ac + bd)/(c^2 + d^2) + (bc - ad)/(c^2 + d^2) i
    T a = storage[0];
    T b = storage[1];
    U c = rhs.real();
    U d = rhs.imag();
    auto denominator = c * c + d * d;
    storage[0] = (a * c + b * d) / denominator;
    storage[1] = (b * c - a * d) / denominator;
    return static_cast<complex<T> &>(*this);
  }

  template<typename U>
  constexpr complex<T> &operator =(const std::complex<U> &rhs) {
    storage[0] = rhs.real();
    storage[1] = rhs.imag();
    return static_cast<complex<T> &>(*this);
  }

#if defined(__CUDACC__) || defined(__HIPCC__)
  template<typename U>
  C10_HOST_DEVICE complex<T> &operator =(const thrust::complex<U> &rhs) {
    storage[0] = rhs.real();
    storage[1] = rhs.imag();
    return static_cast<complex<T> &>(*this);
  }
#endif

  template<typename U>
  explicit constexpr operator std::complex<U>() const {
    return std::complex<U>(std::complex<T>(real(), imag()));
  }

#if defined(__CUDACC__) || defined(__HIPCC__)
  template<typename U>
  explicit operator thrust::complex<U>() const {
    return thrust::complex<U>(thrust::complex<T>(real(), imag()));
  }
#endif

  constexpr T real() const {
    return storage[0];
  }
  constexpr void real(T value) {
    storage[0] = value;
  }
  constexpr T imag() const {
    return storage[1];
  }
  constexpr void imag(T value) {
    storage[1] = value;
  }
};

template<>
struct alignas(4) complex<c10::Half>: public complex_common<c10::Half> {
  using complex_common<c10::Half>::complex_common;
  constexpr complex(): complex_common() {}; // needed by CUDA 9.x
  explicit constexpr complex(const complex<float> &other);
  explicit constexpr complex(const complex<double> &other);
};

template<>
struct alignas(8) complex<float>: public complex_common<float> {
  using complex_common<float>::complex_common;
  constexpr complex(): complex_common() {}; // needed by CUDA 9.x
  constexpr complex(const complex<c10::Half> &other);
  explicit constexpr complex(const complex<double> &other);
};

template<>
struct alignas(16) complex<double>: public complex_common<double> {
  using complex_common<double>::complex_common;
  constexpr complex(): complex_common() {}; // needed by CUDA 9.x
  constexpr complex(const complex<c10::Half> &other);
  constexpr complex(const complex<float> &other);
};

constexpr complex<c10::Half>::complex(const complex<float> &other): complex_common(other.real(), other.imag()) {}
constexpr complex<c10::Half>::complex(const complex<double> &other): complex_common(other.real(), other.imag()) {}
constexpr complex<float>::complex(const complex<c10::Half> &other): complex_common(other.real(), other.imag()) {}
constexpr complex<float>::complex(const complex<double> &other): complex_common(other.real(), other.imag()) {}
constexpr complex<double>::complex(const complex<c10::Half> &other): complex_common(other.real(), other.imag()) {}
constexpr complex<double>::complex(const complex<float> &other): complex_common(other.real(), other.imag()) {}

namespace complex_literals {

constexpr complex<c10::Half> operator"" _ih(long double imag) {
  return complex<c10::Half>(c10::Half(), static_cast<c10::Half>(imag));
}

constexpr complex<float> operator"" _if(long double imag) {
  return complex<float>(float(), static_cast<float>(imag));
}

constexpr complex<double> operator"" _id(long double imag) {
  return complex<double>(double(), static_cast<double>(imag));
}

} // namespace complex_literals

} // namespace c10

template<typename T>
constexpr c10::complex<T> operator+(const c10::complex<T>& val) {
  return val;
}

template<typename T>
constexpr c10::complex<T> operator-(const c10::complex<T>& val) {
  return c10::complex<T>(-val.real(), -val.imag());
}

template<typename T>
constexpr c10::complex<T> operator+(const c10::complex<T>& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result = lhs;
  return result += rhs;
}

template<typename T>
constexpr c10::complex<T> operator+(const c10::complex<T>& lhs, const T& rhs) {
  c10::complex<T> result = lhs;
  return result += rhs;
}

template<typename T>
constexpr c10::complex<T> operator+(const T& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result = rhs;
  return result += lhs;
}

template<typename T>
constexpr c10::complex<T> operator-(const c10::complex<T>& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result = lhs;
  return result -= rhs;
}

template<typename T>
constexpr c10::complex<T> operator-(const c10::complex<T>& lhs, const T& rhs) {
  c10::complex<T> result = lhs;
  return result -= rhs;
}

template<typename T>
constexpr c10::complex<T> operator-(const T& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result = -rhs;
  return result += lhs;
}

template<typename T>
constexpr c10::complex<T> operator*(const c10::complex<T>& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result = lhs;
  return result *= rhs;
}

template<typename T>
constexpr c10::complex<T> operator*(const c10::complex<T>& lhs, const T& rhs) {
  c10::complex<T> result = lhs;
  return result *= rhs;
}

template<typename T>
constexpr c10::complex<T> operator*(const T& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result = rhs;
  return result *= lhs;
}

template<typename T>
constexpr c10::complex<T> operator/(const c10::complex<T>& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result = lhs;
  return result /= rhs;
}

template<typename T>
constexpr c10::complex<T> operator/(const c10::complex<T>& lhs, const T& rhs) {
  c10::complex<T> result = lhs;
  return result /= rhs;
}

template<typename T>
constexpr c10::complex<T> operator/(const T& lhs, const c10::complex<T>& rhs) {
  c10::complex<T> result(lhs, T());
  return result /= rhs;
}

template<typename T>
constexpr bool operator==(const c10::complex<T>& lhs, const c10::complex<T>& rhs) {
  return (lhs.real() == rhs.real()) && (lhs.imag() == rhs.imag());
}

template<typename T>
constexpr bool operator==(const c10::complex<T>& lhs, const T& rhs) {
  return (lhs.real() == rhs) && (lhs.imag() == T());
}

template<typename T>
constexpr bool operator==(const T& lhs, const c10::complex<T>& rhs) {
  return (lhs == rhs.real()) && (T() == rhs.imag());
}

template<typename T>
constexpr bool operator!=(const c10::complex<T>& lhs, const c10::complex<T>& rhs) {
  return !(lhs == rhs);
}

template<typename T>
constexpr bool operator!=(const c10::complex<T>& lhs, const T& rhs) {
  return !(lhs == rhs);
}

template<typename T>
constexpr bool operator!=(const T& lhs, const c10::complex<T>& rhs) {
  return !(lhs == rhs);
}

template <class T, class CharT, class Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const c10::complex<T>& x) {
  return (os << static_cast<std::complex<T>>(x));
}

template <class T, class CharT, class Traits>
std::basic_istream<CharT, Traits>& operator>>(std::basic_istream<CharT, Traits>& is, c10::complex<T>& x) {
  std::complex<T> tmp;
  is >> tmp;
  x = tmp;
  return is;
}

#include <c10/util/complex_math.h>
