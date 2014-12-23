/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    VectorValues.h
 * @brief   Factor Graph Values
 * @author  Richard Roberts
 */

#pragma once

#include <gtsam/base/Vector.h>
#include <gtsam/base/ConcurrentMap.h>
#include <gtsam/base/FastVector.h>
#include <gtsam/global_includes.h>
#include <gtsam/inference/Ordering.h>

#include <boost/shared_ptr.hpp>

namespace gtsam {

  /**
   * This class represents a collection of vector-valued variables associated
   * each with a unique integer index.  It is typically used to store the variables
   * of a GaussianFactorGraph.  Optimizing a GaussianFactorGraph or GaussianBayesNet
   * returns this class.
   *
   * For basic usage, such as receiving a linear solution from gtsam solving functions,
   * or creating this class in unit tests and examples where speed is not important,
   * you can use a simple interface:
   *  - The default constructor VectorValues() to create this class
   *  - insert(Key, const Vector&) to add vector variables
   *  - operator[](Key) for read and write access to stored variables
   *  - \ref exists (Key) to check if a variable is present
   *  - Other facilities like iterators, size(), dim(), etc.
   *
   * Indices can be non-consecutive and inserted out-of-order, but you should not
   * use indices that are larger than a reasonable array size because the indices
   * correspond to positions in an internal array.
   *
   * Example:
   * \code
     VectorValues values;
     values.insert(3, Vector3(1.0, 2.0, 3.0));
     values.insert(4, Vector2(4.0, 5.0));
     values.insert(0, (Vector(4) << 6.0, 7.0, 8.0, 9.0).finished());

     // Prints [ 3.0 4.0 ]
     gtsam::print(values[1]);

     // Prints [ 8.0 9.0 ]
     values[1] = Vector2(8.0, 9.0);
     gtsam::print(values[1]);
     \endcode
   *
   * <h2>Advanced Interface and Performance Information</h2>
   *
   * Internally, all vector values are stored as part of one large vector.  In
   * gtsam this vector is always pre-allocated for efficiency, using the
   * advanced interface described below.  Accessing and modifying already-allocated
   * values is \f$ O(1) \f$.  Using the insert() function of the standard interface
   * is slow because it requires re-allocating the internal vector.
   *
   * For advanced usage, or where speed is important:
   *  - Allocate space ahead of time using a pre-allocating constructor
   *    (\ref AdvancedConstructors "Advanced Constructors"), Zero(),
   *    SameStructure(), resize(), or append().  Do not use
   *    insert(Key, const Vector&), which always has to re-allocate the
   *    internal vector.
   *  - The vector() function permits access to the underlying Vector, for
   *    doing mathematical or other operations that require all values.
   *  - operator[]() returns a SubVector view of the underlying Vector,
   *    without copying any data.
   *
   * Access is through the variable index j, and returns a SubVector,
   * which is a view on the underlying data structure.
   *
   * This class is additionally used in gradient descent and dog leg to store the gradient.
   * \nosubgrouping
   */
  class GTSAM_EXPORT VectorValues {
  protected:
    typedef VectorValues This;
    typedef ConcurrentMap<Key, Vector> Values; ///< Typedef for the collection of Vectors making up a VectorValues
    Values values_; ///< Collection of Vectors making up this VectorValues

  public:
    typedef Values::iterator iterator; ///< Iterator over vector values
    typedef Values::const_iterator const_iterator; ///< Const iterator over vector values
    //typedef Values::reverse_iterator reverse_iterator; ///< Reverse iterator over vector values
    //typedef Values::const_reverse_iterator const_reverse_iterator; ///< Const reverse iterator over vector values
    typedef boost::shared_ptr<This> shared_ptr; ///< shared_ptr to this class
    typedef Values::value_type value_type; ///< Typedef to pair<Key, Vector>, a key-value pair
    typedef value_type KeyValuePair; ///< Typedef to pair<Key, Vector>, a key-value pair
    typedef std::map<Key,size_t> Dims;

    /// @name Standard Constructors
    /// @{

    /**
     * Default constructor creates an empty VectorValues.
     */
    VectorValues() {}

    /** Merge two VectorValues into one, this is more efficient than inserting elements one by one. */
    VectorValues(const VectorValues& first, const VectorValues& second);

    /** Create from another container holding pair<Key,Vector>. */
    template<class CONTAINER>
    explicit VectorValues(const CONTAINER& c) : values_(c.begin(), c.end()) {}

    /** Implicit copy constructor to specialize the explicit constructor from any container. */
    VectorValues(const VectorValues& c) : values_(c.values_) {}

    /** Create from a pair of iterators over pair<Key,Vector>. */
    template<typename ITERATOR>
    VectorValues(ITERATOR first, ITERATOR last) : values_(first, last) {}

    /** Constructor from Vector. */
    VectorValues(const Vector& c, const Dims& dims);

    /** Create a VectorValues with the same structure as \c other, but filled with zeros. */
    static VectorValues Zero(const VectorValues& other);

    /// @}
    /// @name Standard Interface
    /// @{

    /** Number of variables stored. */
    size_t size() const { return values_.size(); }

    /** Return the dimension of variable \c j. */
    size_t dim(Key j) const { return at(j).rows(); }

    /** Check whether a variable with key \c j exists. */
    bool exists(Key j) const { return find(j) != end(); }

    /** Read/write access to the vector value with key \c j, throws std::out_of_range if \c j does not exist, identical to operator[](Key). */
    Vector& at(Key j) {
      iterator item = find(j);
      if(item == end())
        throw std::out_of_range(
        "Requested variable '" + DefaultKeyFormatter(j) + "' is not in this VectorValues.");
      else
        return item->second;
    }

    /** Access the vector value with key \c j (const version), throws std::out_of_range if \c j does not exist, identical to operator[](Key). */
    const Vector& at(Key j) const {
      const_iterator item = find(j);
      if(item == end())
        throw std::out_of_range(
        "Requested variable '" + DefaultKeyFormatter(j) + "' is not in this VectorValues.");
      else
        return item->second;
    }

    /** Read/write access to the vector value with key \c j, throws std::out_of_range if \c j does
    *   not exist, identical to at(Key). */
    Vector& operator[](Key j) { return at(j); }

    /** Access the vector value with key \c j (const version), throws std::out_of_range if \c j does
    *   not exist, identical to at(Key). */
    const Vector& operator[](Key j) const { return at(j); }

    /** For all key/value pairs in \c values, replace values with corresponding keys in this class
    *   with those in \c values.  Throws std::out_of_range if any keys in \c values are not present
    *   in this class. */
    void update(const VectorValues& values);

    /** Insert a vector \c value with key \c j.  Throws an invalid_argument exception if the key \c
     *  j is already used.
     * @param value The vector to be inserted.
     * @param j The index with which the value will be associated. */
    iterator insert(Key j, const Vector& value) {
      return insert(std::make_pair(j, value)); // Note only passing a reference to the Vector
    }

    /** Insert a vector \c value with key \c j.  Throws an invalid_argument exception if the key \c
     *  j is already used.
     * @param value The vector to be inserted.
     * @param j The index with which the value will be associated. */
    iterator insert(const std::pair<Key, Vector>& key_value) {
      // Note that here we accept a pair with a reference to the Vector, but the Vector is copied as
      // it is inserted into the values_ map.
      std::pair<iterator, bool> result = values_.insert(key_value);
      if(!result.second)
        throw std::invalid_argument(
        "Requested to insert variable '" + DefaultKeyFormatter(key_value.first)
        + "' already in this VectorValues.");
      return result.first;
    }

    /** Insert all values from \c values.  Throws an invalid_argument exception if any keys to be
     *  inserted are already used. */
    void insert(const VectorValues& values);

    /** insert that mimics the STL map insert - if the value already exists, the map is not modified
     *  and an iterator to the existing value is returned, along with 'false'.  If the value did not
     *  exist, it is inserted and an iterator pointing to the new element, along with 'true', is
     *  returned. */
    std::pair<iterator, bool> tryInsert(Key j, const Vector& value) {
      return values_.insert(std::make_pair(j, value)); }

    /** Erase the vector with the given key, or throw std::out_of_range if it does not exist */
    void erase(Key var) {
      if(values_.unsafe_erase(var) == 0)
        throw std::invalid_argument("Requested variable '" + DefaultKeyFormatter(var) + "', is not in this VectorValues.");
    }

    /** Set all values to zero vectors. */
    void setZero();

    iterator begin()                      { return values_.begin(); }  ///< Iterator over variables
    const_iterator begin() const          { return values_.begin(); }  ///< Iterator over variables
    iterator end()                        { return values_.end(); }    ///< Iterator over variables
    const_iterator end() const            { return values_.end(); }    ///< Iterator over variables
    //reverse_iterator rbegin()             { return values_.rbegin(); } ///< Reverse iterator over variables
    //const_reverse_iterator rbegin() const { return values_.rbegin(); } ///< Reverse iterator over variables
    //reverse_iterator rend()               { return values_.rend(); }   ///< Reverse iterator over variables
    //const_reverse_iterator rend() const   { return values_.rend(); }   ///< Reverse iterator over variables

    /** Return the iterator corresponding to the requested key, or end() if no variable is present with this key. */
    iterator find(Key j) { return values_.find(j); }

    /** Return the iterator corresponding to the requested key, or end() if no variable is present with this key. */
    const_iterator find(Key j) const { return values_.find(j); }

    /** print required by Testable for unit testing */
    void print(const std::string& str = "VectorValues: ",
        const KeyFormatter& formatter = DefaultKeyFormatter) const;

    /** equals required by Testable for unit testing */
    bool equals(const VectorValues& x, double tol = 1e-9) const;

    /// @{
    /// @name Advanced Interface
    /// @{

    /** Retrieve the entire solution as a single vector */
    Vector vector() const;

    /** Access a vector that is a subset of relevant keys. */
    Vector vector(const FastVector<Key>& keys) const;

    /** Access a vector that is a subset of relevant keys, dims version. */
    Vector vector(const Dims& dims) const;

    /** Swap the data in this VectorValues with another. */
    void swap(VectorValues& other);

    /** Check if this VectorValues has the same structure (keys and dimensions) as another */
    bool hasSameStructure(const VectorValues other) const;

    /// @}
    /// @name Linear algebra operations
    /// @{

    /** Dot product with another VectorValues, interpreting both as vectors of
    * their concatenated values.  Both VectorValues must have the
    * same structure (checked when NDEBUG is not defined). */
    double dot(const VectorValues& v) const;

    /** Vector L2 norm */
    double norm() const;

    /** Squared vector L2 norm */
    double squaredNorm() const;

    /** Element-wise addition, synonym for add().  Both VectorValues must have the same structure
     *  (checked when NDEBUG is not defined). */
    VectorValues operator+(const VectorValues& c) const;

    /** Element-wise addition, synonym for operator+().  Both VectorValues must have the same
     *  structure (checked when NDEBUG is not defined). */
    VectorValues add(const VectorValues& c) const;

    /** Element-wise addition in-place, synonym for operator+=().  Both VectorValues must have the
     * same structure (checked when NDEBUG is not defined). */
    VectorValues& operator+=(const VectorValues& c);

    /** Element-wise addition in-place, synonym for operator+=().  Both VectorValues must have the
     * same structure (checked when NDEBUG is not defined). */
    VectorValues& addInPlace(const VectorValues& c);

    /** Element-wise addition in-place, but allows for empty slots in *this. Slower */
    VectorValues& addInPlace_(const VectorValues& c);

    /** Element-wise subtraction, synonym for subtract().  Both VectorValues must have the same
     *  structure (checked when NDEBUG is not defined). */
    VectorValues operator-(const VectorValues& c) const;

    /** Element-wise subtraction, synonym for operator-().  Both VectorValues must have the same
     *  structure (checked when NDEBUG is not defined). */
    VectorValues subtract(const VectorValues& c) const;

    /** Element-wise scaling by a constant. */
    friend GTSAM_EXPORT VectorValues operator*(const double a, const VectorValues &v);

    /** Element-wise scaling by a constant. */
    VectorValues scale(const double a) const;

    /** Element-wise scaling by a constant in-place. */
    VectorValues& operator*=(double alpha);

    /** Element-wise scaling by a constant in-place. */
    VectorValues& scaleInPlace(double alpha);

    /// @}

    /// @}
    /// @name Matlab syntactic sugar for linear algebra operations
    /// @{

    //inline VectorValues scale(const double a, const VectorValues& c) const { return a * (*this); }

    /// @}

    /**
     * scale a vector by a scalar
     */
    //friend VectorValues operator*(const double a, const VectorValues &v) {
    //  VectorValues result = VectorValues::SameStructure(v);
    //  for(Key j = 0; j < v.size(); ++j)
    //    result.values_[j] = a * v.values_[j];
    //  return result;
    //}

    //// TODO: linear algebra interface seems to have been added for SPCG.
    //friend void axpy(double alpha, const VectorValues& x, VectorValues& y) {
    //  if(x.size() != y.size())
    //    throw std::invalid_argument("axpy(VectorValues) called with different vector sizes");
    //  for(Key j = 0; j < x.size(); ++j)
    //    if(x.values_[j].size() == y.values_[j].size())
    //      y.values_[j] += alpha * x.values_[j];
    //    else
    //      throw std::invalid_argument("axpy(VectorValues) called with different vector sizes");
    //}
    //// TODO: linear algebra interface seems to have been added for SPCG.
    //friend void sqrt(VectorValues &x) {
    //  for(Key j = 0; j < x.size(); ++j)
    //    x.values_[j] = x.values_[j].cwiseSqrt();
    //}

    //// TODO: linear algebra interface seems to have been added for SPCG.
    //friend void ediv(const VectorValues& numerator, const VectorValues& denominator, VectorValues &result) {
    //  if(numerator.size() != denominator.size() || numerator.size() != result.size())
    //    throw std::invalid_argument("ediv(VectorValues) called with different vector sizes");
    //  for(Key j = 0; j < numerator.size(); ++j)
    //    if(numerator.values_[j].size() == denominator.values_[j].size() && numerator.values_[j].size() == result.values_[j].size())
    //      result.values_[j] = numerator.values_[j].cwiseQuotient(denominator.values_[j]);
    //    else
    //      throw std::invalid_argument("ediv(VectorValues) called with different vector sizes");
    //}

    //// TODO: linear algebra interface seems to have been added for SPCG.
    //friend void edivInPlace(VectorValues& x, const VectorValues& y) {
    //  if(x.size() != y.size())
    //    throw std::invalid_argument("edivInPlace(VectorValues) called with different vector sizes");
    //  for(Key j = 0; j < x.size(); ++j)
    //    if(x.values_[j].size() == y.values_[j].size())
    //      x.values_[j].array() /= y.values_[j].array();
    //    else
    //      throw std::invalid_argument("edivInPlace(VectorValues) called with different vector sizes");
    //}

  private:
    /** Serialization function */
    friend class boost::serialization::access;
    template<class ARCHIVE>
    void serialize(ARCHIVE & ar, const unsigned int version) {
      ar & BOOST_SERIALIZATION_NVP(values_);
    }
  }; // VectorValues definition

} // \namespace gtsam
