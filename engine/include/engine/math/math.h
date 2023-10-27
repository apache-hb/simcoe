#pragma once

#include <cmath>
#include <numbers>

#include "engine/core/panic.h"

namespace simcoe::math {
    template<typename T>
    constexpr T kPi = std::numbers::pi_v<T>;

    template<typename T>
    constexpr T kRadToDeg = T(180) / kPi<T>;

    template<typename T>
    constexpr T kDegToRad = kPi<T> / T(180);

    template<typename T>
    T clamp(T it, T low, T high) {
        if (it < low)
            return low;

        if (it > high)
            return high;

        return it;
    }

    template<typename T>
    T wrapAngle(T it) {
        if (it > std::numbers::pi_v<T>)
            return it - std::numbers::pi_v<T> * T(2);

        if (it < -std::numbers::pi_v<T>)
            return it + std::numbers::pi_v<T> * T(2);

        return it;
    }

    template<typename T>
    T angleDelta(T lhs, T rhs) {
        auto delta = lhs - rhs;
        return wrapAngle(delta);
    }

    /**
     * vector types are organized like so
     *
     * struct Type {
     *    fields...
     *
     *    constructors
     *    static `from` constructors
     *    comparison operators
     *    arithmetic operators
     *    in place arithmetic operators
     *    conversion operators
     *    vector specific functions
     * }
     */

    template<typename T>
    struct Vec2;

    template<typename T>
    struct Vec3;

    template<typename T>
    struct Vec4;

    template<typename T>
    struct Resolution {
        T width;
        T height;

        constexpr Resolution() : Resolution(T(0)) { }
        constexpr Resolution(T width, T height) : width(width), height(height) { }
        constexpr Resolution(T it) : Resolution(it, it) { }

        constexpr static Resolution from(T width, T height) { return { width, height }; }
        constexpr static Resolution from(T it) { return from(it, it); }
        constexpr static Resolution from(const T *pData) { return from(pData[0], pData[1]); }

        constexpr static Resolution zero() { return from(T(0)); }
        constexpr static Resolution unit() { return from(T(1)); }

        constexpr bool operator==(const Resolution &other) const = default;
        constexpr bool operator!=(const Resolution &other) const = default;

        template<typename O>
        constexpr Resolution<O> as() const { return Resolution<O>::from(O(width), O(height)); }

        template<typename U>
        constexpr U aspectRatio() const {
            auto [w, h] = as<U>();
            return w / h;
        }

        constexpr operator Vec2<T>() const {
            return { width, height };
        }
    };

    template<typename T>
    struct Vec2 {
        T x;
        T y;

        constexpr Vec2() : Vec2(T(0)) { }
        constexpr Vec2(T x, T y) : x(x), y(y) { }
        constexpr Vec2(T it) : Vec2(it, it) { }

        constexpr static Vec2 from(T x, T y) { return { x, y }; }
        constexpr static Vec2 from(T it) { return from(it, it); }
        constexpr static Vec2 from(const T *pData) { return from(pData[0], pData[1]); }

        constexpr static Vec2 zero() { return from(T(0)); }
        constexpr static Vec2 unit() { return from(T(1)); }

        constexpr bool operator==(const Vec2& other) const = default;
        constexpr bool operator!=(const Vec2& other) const = default;

        constexpr Vec2 operator+(const Vec2& other) const { return from(x + other.x, y + other.y); }
        constexpr Vec2 operator-(const Vec2 &other) const { return from(x - other.x, y - other.y); }
        constexpr Vec2 operator*(const Vec2& other) const { return from(x * other.x, y * other.y); }
        constexpr Vec2 operator/(const Vec2& other) const { return from(x / other.x, y / other.y); }

        constexpr Vec2 operator+=(const Vec2& other) { return *this = *this + other; }
        constexpr Vec2 operator-=(const Vec2& other) { return *this = *this - other; }
        constexpr Vec2 operator*=(const Vec2& other) { return *this = *this * other; }
        constexpr Vec2 operator/=(const Vec2& other) { return *this = *this / other; }

        template<typename O>
        constexpr Vec2<O> as() const { return Vec2<O>::from(O(x), O(y)); }

        bool isinf() const { return std::isinf(x) || std::isinf(y); }

        constexpr Vec2 negate() const { return from(-x, -y); }
        constexpr T length() const { return std::sqrt(x * x + y * y); }

        constexpr Vec2 normal() const {
            auto len = length();
            return from(x / len, y / len);
        }

        template<typename O>
        constexpr Vec2<O> floor() const { return Vec2<O>::from(O(std::floor(x)), O(std::floor(y))); }

        template<typename O>
        constexpr Vec2<O> ceil() const { return Vec2<O>::from(O(std::ceil(x)), O(std::ceil(y))); }

        constexpr Vec2 clamp(const Vec2 &low, const Vec2 &high) const {
            return clamp(*this, low, high);
        }

        constexpr Vec2 clamp(T low, T high) const {
            return clamp(*this, low, high);
        }

        static constexpr Vec2 clamp(const Vec2 &it, const Vec2 &low, const Vec2 &high) {
            return from(math::clamp(it.x, low.x, high.x), math::clamp(it.y, low.y, high.y));
        }

        static constexpr Vec2 clamp(const Vec2 &it, T low, T high) {
            return clamp(it, from(low), from(high));
        }

        constexpr T *data() { return &x; } // TODO: this is UB
        constexpr const T *data() const { return &x; }
    };

    template<typename T>
    struct Vec3 {
        T x;
        T y;
        T z;

        constexpr Vec3() : Vec3(T(0)) { }
        constexpr Vec3(T x, T y, T z) : x(x), y(y), z(z) { }
        constexpr Vec3(T it) : Vec3(it, it, it){ }

        static constexpr Vec3 from(T x, T y, T z) { return { x, y, z }; }
        static constexpr Vec3 from(T it) { return from(it, it, it); }
        static constexpr Vec3 from(T x, Vec2<T> yz) { return from(x, yz.x, yz.y); }
        static constexpr Vec3 from(Vec2<T> xy, T z) { return from(xy.x, xy.y, z); }
        static constexpr Vec3 from(const T *pData) { return from(pData[0], pData[1], pData[2]); }

        static constexpr Vec3 zero() { return from(T(0)); }
        static constexpr Vec3 unit() { return from(T(1)); }

        constexpr bool operator==(const Vec3& other) const = default;
        constexpr bool operator!=(const Vec3& it) const = default;

        constexpr Vec3 operator+(const Vec3& it) const { return from(x + it.x, y + it.y, z + it.z); }
        constexpr Vec3 operator-(const Vec3& it) const { return from(x - it.x, y - it.y, z - it.z); }
        constexpr Vec3 operator*(const Vec3& it) const { return from(x * it.x, y * it.y, z * it.z); }
        constexpr Vec3 operator/(const Vec3& it) const { return from(x / it.x, y / it.y, z / it.z); }

        constexpr Vec3 operator+=(const Vec3& it) { return *this = *this + it; }
        constexpr Vec3 operator-=(const Vec3& it) { return *this = *this - it; }
        constexpr Vec3 operator*=(const Vec3& it) { return *this = *this * it; }
        constexpr Vec3 operator/=(const Vec3& it) { return *this = *this / it; }

        template<typename O>
        constexpr Vec3<O> as() const { return Vec3<O>::from(O(x), O(y), O(z)); }

        constexpr Vec2<T> xy() const { return Vec2<T>::from(x, y); }
        constexpr Vec2<T> xz() const { return Vec2<T>::from(x, z); }
        constexpr Vec2<T> yz() const { return Vec2<T>::from(y, z); }

        bool isinf() const { return std::isinf(x) || std::isinf(y) || std::isinf(z); }

        constexpr bool isUniform() const { return x == y && y == z; }

        constexpr Vec3 negate() const { return from(-x, -y, -z); }
        constexpr T length() const { return std::sqrt(x * x + y * y + z * z); }

        constexpr Vec3 normal() const {
            auto len = length();
            return from(x / len, y / len, z / len);
        }

        static constexpr Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
            return from(
                lhs.y * rhs.z - lhs.z * rhs.y,
                lhs.z * rhs.x - lhs.x * rhs.z,
                lhs.x * rhs.y - lhs.y * rhs.x
            );
        }

        static constexpr T dot(const Vec3& lhs, const Vec3& rhs) {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        static constexpr Vec3 rotate(const Vec3& point, const Vec3& origin, const Vec3& rotate) {
            auto [x, y, z] = point - origin;

            auto sinX = std::sin(rotate.x);
            auto cosX = std::cos(rotate.x);

            auto sinY = std::sin(rotate.y);
            auto cosY = std::cos(rotate.y);

            auto sinZ = std::sin(rotate.z);
            auto cosZ = std::cos(rotate.z);

            auto x1 = x * cosZ - y * sinZ;
            auto y1 = x * sinZ + y * cosZ;

            auto x2 = x1 * cosY + z * sinY;
            auto z2 = x1 * -sinY + z * cosY;

            auto y3 = y1 * cosX - z2 * sinX;
            auto z3 = y1 * sinX + z2 * cosX;

            return from(x2, y3, z3) + origin;
        }

        constexpr T *data() { return &x; } // TODO: this is UB
        constexpr const T *data() const { return &x; }

    private:
        static constexpr T Vec3::*components[] { &Vec3::x, &Vec3::y, &Vec3::z };
    };

    template<typename T>
    struct Vec4 {
        T x;
        T y;
        T z;
        T w;

        constexpr Vec4() : Vec4(T(0)) { }
        constexpr Vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) { }
        constexpr Vec4(T it) : Vec4(it, it, it, it) { }

        static constexpr Vec4 from(T x, T y, T z, T w) { return { x, y, z, w }; }
        static constexpr Vec4 from(T it) { return from(it, it, it, it); }
        static constexpr Vec4 from(const T *pData) { return from(pData[0], pData[1], pData[2], pData[3]); }

        static constexpr Vec4 from(Vec3<T> o, T w) { return from(o.x, o.y, o.z, w); }

        static constexpr Vec4 zero() { return from(T(0)); }
        static constexpr Vec4 unit() { return from(T(1)); }

        constexpr bool operator==(const Vec4& other) const = default;
        constexpr bool operator!=(const Vec4& other) const = default;

        constexpr Vec4 operator+(const Vec4& other) const { return from(x + other.x, y + other.y, z + other.z, w + other.w); }
        constexpr Vec4 operator-(const Vec4& other) const { return from(x - other.x, y - other.y, z - other.z, w - other.w); }
        constexpr Vec4 operator*(const Vec4& other) const { return from(x * other.x, y * other.y, z * other.z, w * other.w); }
        constexpr Vec4 operator/(const Vec4& other) const { return from(x / other.x, y / other.y, z / other.z, w / other.w); }

        constexpr Vec4 operator+=(const Vec4& other) { return *this = *this + other; }
        constexpr Vec4 operator-=(const Vec4& other) { return *this = *this - other; }
        constexpr Vec4 operator*=(const Vec4& other) { return *this = *this * other; }
        constexpr Vec4 operator/=(const Vec4& other) { return *this = *this / other; }

        template<typename O>
        constexpr Vec4<O> as() const { return Vec4<O>::from(O(x), O(y), O(z), O(w)); }

        constexpr Vec3<T> xyz() const { return Vec3<T>::from(x, y, z); }

        bool isinf() const { return std::isinf(x) || std::isinf(y) || std::isinf(z) || std::isinf(w); }

        constexpr T length() const { return std::sqrt(x * x + y * y + z * z + w * w); }
        constexpr Vec4 negate() const { return from(-x, -y, -z, -w); }

        constexpr Vec4 normal() const {
            auto len = length();
            return from(x / len, y / len, z / len, w / len);
        }

        constexpr const T& operator[](size_t index) const { return at(index); }
        constexpr T& operator[](size_t index) { return at(index); }

        constexpr const T& at(size_t index) const { return this->*components[index];}
        constexpr T& at(size_t index) { return this->*components[index]; }

        constexpr T *data() { return &x; } // TODO: this is UB
        constexpr const T *data() const { return &x; }
    private:
        static constexpr T Vec4::*components[] { &Vec4::x, &Vec4::y, &Vec4::z, &Vec4::w };
    };

    template<typename T>
    struct Mat3x3 {
        using Row = Vec3<T>;
        Row rows[3];

        constexpr Mat3x3() : Mat3x3(T(0)) { }
        constexpr Mat3x3(T it) : Mat3x3(Row::from(it)) { }
        constexpr Mat3x3(const Row& row) : Mat3x3(row, row, row) { }
        constexpr Mat3x3(const Row& row0, const Row& row1, const Row& row2) : rows{ row0, row1, row2 } { }

        static constexpr Mat3x3 from(const Row& row0, const Row& row1, const Row& row2) {
            return { row0, row1, row2 };
        }

        static constexpr Mat3x3 from(T it) {
            return from(Row::from(it), Row::from(it), Row::from(it));
        }

        static constexpr Mat3x3 identity() {
            auto row1 = Row::from(1, 0, 0);
            auto row2 = Row::from(0, 1, 0);
            auto row3 = Row::from(0, 0, 1);
            return from(row1, row2, row3);
        }
    };

    template<typename T>
    struct Mat4x4 {
        using Row = Vec4<T>;
        using Row3 = Vec3<T>;
        Row rows[4];

        constexpr Mat4x4() : Mat4x4(T(0)) { }
        constexpr Mat4x4(T it) : Mat4x4(Row::from(it)) { }
        constexpr Mat4x4(const Row& row) : Mat4x4(row, row, row, row) { }
        constexpr Mat4x4(const Row& row0, const Row& row1, const Row& row2, const Row& row3) : rows{ row0, row1, row2, row3 } { }

        constexpr static Mat4x4 from(const Row& row0, const Row& row1, const Row& row2, const Row& row3) {
            return { row0, row1, row2, row3 };
        }

        constexpr static Mat4x4 from(T it) {
            return from(Row::from(it), Row::from(it), Row::from(it), Row::from(it));
        }

        constexpr static Mat4x4 from(const T *pData) {
            return from(Row::from(pData), Row::from(pData + 4), Row::from(pData + 8), Row::from(pData + 12));
        }

        constexpr Row column(size_t column) const {
            return Row::from(at(column, 0), at(column, 1), at(column, 2), at(column, 3));
        }

        constexpr Row row(size_t row) const {
            return at(row);
        }

        constexpr const Row& at(size_t it) const { return rows[it]; }
        constexpr Row& at(size_t it) { return rows[it]; }

        constexpr const Row& operator[](size_t row) const { return rows[row];}
        constexpr Row& operator[](size_t row) { return rows[row]; }

        constexpr const T &at(size_t it, size_t col) const { return at(it).at(col); }
        constexpr T &at(size_t it, size_t col) { return at(it).at(col); }

        constexpr Row mul(const Row& other) const {
            auto row0 = at(0);
            auto row1 = at(1);
            auto row2 = at(2);
            auto row3 = at(3);

            auto x = row0.x * other.x + row0.y * other.y + row0.z * other.z + row0.w * other.w;
            auto y = row1.x * other.x + row1.y * other.y + row1.z * other.z + row1.w * other.w;
            auto z = row2.x * other.x + row2.y * other.y + row2.z * other.z + row2.w * other.w;
            auto w = row3.x * other.x + row3.y * other.y + row3.z * other.z + row3.w * other.w;

            return Row::from(x, y, z, w);
        }

        constexpr Mat4x4 mul(const Mat4x4& other) const {
            auto row0 = at(0);
            auto row1 = at(1);
            auto row2 = at(2);
            auto row3 = at(3);

            auto other0 = other.at(0);
            auto other1 = other.at(1);
            auto other2 = other.at(2);
            auto other3 = other.at(3);

            auto out0 = Row::from(
                (other0.x * row0.x) + (other1.x * row0.y) + (other2.x * row0.z) + (other3.x * row0.w),
                (other0.y * row0.x) + (other1.y * row0.y) + (other2.y * row0.z) + (other3.y * row0.w),
                (other0.z * row0.x) + (other1.z * row0.y) + (other2.z * row0.z) + (other3.z * row0.w),
                (other0.w * row0.x) + (other1.w * row0.y) + (other2.w * row0.z) + (other3.w * row0.w)
            );

            auto out1 = Row::from(
                (other0.x * row1.x) + (other1.x * row1.y) + (other2.x * row1.z) + (other3.x * row1.w),
                (other0.y * row1.x) + (other1.y * row1.y) + (other2.y * row1.z) + (other3.y * row1.w),
                (other0.z * row1.x) + (other1.z * row1.y) + (other2.z * row1.z) + (other3.z * row1.w),
                (other0.w * row1.x) + (other1.w * row1.y) + (other2.w * row1.z) + (other3.w * row1.w)
            );

            auto out2 = Row::from(
                (other0.x * row2.x) + (other1.x * row2.y) + (other2.x * row2.z) + (other3.x * row2.w),
                (other0.y * row2.x) + (other1.y * row2.y) + (other2.y * row2.z) + (other3.y * row2.w),
                (other0.z * row2.x) + (other1.z * row2.y) + (other2.z * row2.z) + (other3.z * row2.w),
                (other0.w * row2.x) + (other1.w * row2.y) + (other2.w * row2.z) + (other3.w * row2.w)
            );

            auto out3 = Row::from(
                (other0.x * row3.x) + (other1.x * row3.y) + (other2.x * row3.z) + (other3.x * row3.w),
                (other0.y * row3.x) + (other1.y * row3.y) + (other2.y * row3.z) + (other3.y * row3.w),
                (other0.z * row3.x) + (other1.z * row3.y) + (other2.z * row3.z) + (other3.z * row3.w),
                (other0.w * row3.x) + (other1.w * row3.y) + (other2.w * row3.z) + (other3.w * row3.w)
            );

            return Mat4x4::from(out0, out1, out2, out3);
        }

        constexpr Mat4x4 add(const Mat4x4& other) const {
            return Mat4x4::from(
                at(0).add(other.at(0)),
                at(1).add(other.at(1)),
                at(2).add(other.at(2)),
                at(3).add(other.at(3))
            );
        }

        constexpr Mat4x4 operator*(const Mat4x4& other) const {
            return mul(other);
        }

        constexpr Mat4x4 operator*=(const Mat4x4& other) {
            return *this = *this * other;
        }

        constexpr Mat4x4 operator+(const Mat4x4& other) const {
            return add(other);
        }

        constexpr Mat4x4 operator+=(const Mat4x4& other) {
            return *this = *this + other;
        }

        ///
        /// scaling related functions
        ///

        static constexpr Mat4x4 scaling(const Row3& scale) {
            return scaling(scale.x, scale.y, scale.z);
        }

        static constexpr Mat4x4 scaling(T x, T y, T z) {
            auto row0 = Row::from(x, 0, 0, 0);
            auto row1 = Row::from(0, y, 0, 0);
            auto row2 = Row::from(0, 0, z, 0);
            auto row3 = Row::from(0, 0, 0, 1);
            return from(row0, row1, row2, row3);
        }

        constexpr Row3 getScale() const {
            return Row3::from(at(0, 0), at(1, 1), at(2, 2));
        }

        constexpr void setScale(const Row3& scale) {
            at(0, 0) = scale.x;
            at(1, 1) = scale.y;
            at(2, 2) = scale.z;
        }

        ///
        /// translation related functions
        ///

        static constexpr Mat4x4 translation(const Row3& translation) {
            return Mat4x4::translation(translation.x, translation.y, translation.z);
        }

        static constexpr Mat4x4 translation(T x, T y, T z) {
            auto row0 = Row::from(1, 0, 0, x);
            auto row1 = Row::from(0, 1, 0, y);
            auto row2 = Row::from(0, 0, 1, z);
            auto row3 = Row::from(0, 0, 0, 1);
            return from(row0, row1, row2, row3);
        }

        constexpr Row3 getTranslation() const {
            return Row3::from(at(0, 3), at(1, 3), at(2, 3));
        }

        constexpr void setTranslation(const Row3& translation) {
            at(0, 3) = translation.x;
            at(1, 3) = translation.y;
            at(2, 3) = translation.z;
        }

        static constexpr Mat4x4 rotation(const Row3& rotation) {
            auto& [pitch, yaw, roll] = rotation;
            const T cp = std::cos(pitch);
            const T sp = std::sin(pitch);

            const T cy = std::cos(yaw);
            const T sy = std::sin(yaw);

            const T cr = std::cos(roll);
            const T sr = std::sin(roll);

            auto r0 = Row::from(
                cr * cy + sr * sp * sy,
                sr * cp,
                sr * sp * cy - cr * sy,
                0
            );

            auto r1 = Row::from(
                cr * sp * sy - sr * cy,
                cr * cp,
                sr * sy + cr * sp * cy,
                0
            );

            auto r2 = Row::from(
                cp * sy,
                -sp,
                cp * cy,
                0
            );

            auto r3 = Row::from(0, 0, 0, 1);

            return from(r0, r1, r2, r3);
        }

        constexpr Mat4x4 transpose() const {
            auto r0 = Row::from(rows[0].x, rows[1].x, rows[2].x, rows[3].x);
            auto r1 = Row::from(rows[0].y, rows[1].y, rows[2].y, rows[3].y);
            auto r2 = Row::from(rows[0].z, rows[1].z, rows[2].z, rows[3].z);
            auto r3 = Row::from(rows[0].w, rows[1].w, rows[2].w, rows[3].w);
            return from(r0, r1, r2, r3);
        }

        static constexpr Mat4x4 identity() {
            auto row0 = Row::from(1, 0, 0, 0);
            auto row1 = Row::from(0, 1, 0, 0);
            auto row2 = Row::from(0, 0, 1, 0);
            auto row3 = Row::from(0, 0, 0, 1);
            return from(row0, row1, row2, row3);
        }

        static constexpr Mat4x4 lookToLH(const Row3& eye, const Row3& dir, const Row3& up) {
            ASSERT(eye != Row3::zero());
            ASSERT(up != Row3::zero());

            ASSERT(!eye.isinf());
            ASSERT(!up.isinf());

            auto r2 = dir.normal();
            auto r0 = Row3::cross(up, r2).normal();
            auto r1 = Row3::cross(r2, r0);

            auto negEye = eye.negate();

            auto d0 = Row3::dot(r0, negEye);
            auto d1 = Row3::dot(r1, negEye);
            auto d2 = Row3::dot(r2, negEye);

            auto s0 = Row::from(r0, d0);
            auto s1 = Row::from(r1, d1);
            auto s2 = Row::from(r2, d2);
            auto s3 = Row::from(0, 0, 0, 1);

            return Mat4x4::from(s0, s1, s2, s3).transpose();
        }

        static constexpr Mat4x4 lookToRH(const Row3& eye, const Row3& dir, const Row3& up) {
            return lookToLH(eye, dir.negate(), up);
        }

        static constexpr Mat4x4 lookAtRH(const Row3& eye, const Row3& focus, const Row3& up) {
            return lookToLH(eye, eye - focus, up);
        }

        static constexpr Mat4x4 perspectiveRH(T fov, T aspect, T nearLimit, T farLimit) {
            auto fovSin = std::sin(fov / 2);
            auto fovCos = std::cos(fov / 2);

            auto height = fovCos / fovSin;
            auto width = height / aspect;
            auto range = farLimit / (nearLimit - farLimit);

            auto r0 = Row::from(width, 0, 0, 0);
            auto r1 = Row::from(0, height, 0, 0);
            auto r2 = Row::from(0, 0, range, -1);
            auto r3 = Row::from(0, 0, range * nearLimit, 0);
            return from(r0, r1, r2, r3);
        }

        static constexpr Mat4x4 orthographicRH(T width, T height, T nearLimit, T farLimit) {
            auto range = 1 / (nearLimit - farLimit);

            auto r0 = Row::from(2 / width, 0, 0, 0);
            auto r1 = Row::from(0, 2 / height, 0, 0);
            auto r2 = Row::from(0, 0, range, 0);
            auto r3 = Row::from(0, 0, range * nearLimit, 1);
            return from(r0, r1, r2, r3);
        }
    };

    using int2 = Vec2<int>;
    using int3 = Vec3<int>;
    using int4 = Vec4<int>;
    static_assert(sizeof(int2) == sizeof(int) * 2);
    static_assert(sizeof(int3) == sizeof(int) * 3);
    static_assert(sizeof(int4) == sizeof(int) * 4);


    using uint2 = Vec2<uint32_t>;
    using uint3 = Vec3<uint32_t>;
    using uint4 = Vec4<uint32_t>;
    static_assert(sizeof(uint2) == sizeof(uint32_t) * 2);
    static_assert(sizeof(uint3) == sizeof(uint32_t) * 3);
    static_assert(sizeof(uint4) == sizeof(uint32_t) * 4);


    using size2 = Vec2<size_t>;
    using size3 = Vec3<size_t>;
    using size4 = Vec4<size_t>;
    static_assert(sizeof(size2) == sizeof(size_t) * 2);
    static_assert(sizeof(size3) == sizeof(size_t) * 3);
    static_assert(sizeof(size4) == sizeof(size_t) * 4);

    using float2 = Vec2<float>;
    using float3 = Vec3<float>;
    using float4 = Vec4<float>;
    using float3x3 = Mat3x3<float>;
    using float4x4 = Mat4x4<float>;
    static_assert(sizeof(float2) == sizeof(float) * 2);
    static_assert(sizeof(float3) == sizeof(float) * 3);
    static_assert(sizeof(float4) == sizeof(float) * 4);
    static_assert(sizeof(float3x3) == sizeof(float) * 3 * 3);
    static_assert(sizeof(float4x4) == sizeof(float) * 4 * 4);
}

#define VEC2_HASH(T) \
template<> \
struct std::hash<simcoe::math::Vec2<T>> { \
    size_t operator()(const simcoe::math::Vec2<T>& it) const { \
        return std::hash<T>()(it.x) ^ std::hash<T>()(it.y); \
    } \
};

#define VEC3_HASH(T) \
template<> \
struct std::hash<simcoe::math::Vec3<T>> { \
    size_t operator()(const simcoe::math::Vec3<T>& it) const { \
        return std::hash<T>()(it.x) ^ std::hash<T>()(it.y) ^ std::hash<T>()(it.z); \
    } \
};

#define VEC4_HASH(T) \
template<> \
struct std::hash<simcoe::math::Vec4<T>> { \
    size_t operator()(const simcoe::math::Vec4<T>& it) const { \
        return std::hash<T>()(it.x) ^ std::hash<T>()(it.y) ^ std::hash<T>()(it.z) ^ std::hash<T>()(it.w); \
    } \
};

#define VEC_HASH(T) \
VEC2_HASH(T) \
VEC3_HASH(T) \
VEC4_HASH(T)

VEC_HASH(int)
VEC_HASH(uint32_t)
VEC_HASH(size_t)
VEC_HASH(float)
