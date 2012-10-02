// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_GEOMETRY_AXISALIGNED_CUBE_GEOMETRY_HH
#define DUNE_GEOMETRY_AXISALIGNED_CUBE_GEOMETRY_HH

/** \file
    \brief A geometry implementation for axis-aligned hypercubes
 */

#include <bitset>

#include <dune/common/fvector.hh>
#include <dune/common/fmatrix.hh>
#include <dune/common/diagonalmatrix.hh>
#include <dune/common/unused.hh>

#include <dune/geometry/type.hh>




namespace Dune {

  /** \brief A geometry implementation for axis-aligned hypercubes
   *
   * This code is much faster than a generic implementation for hexahedral elements.
   * All methods use the fact that a geometry for axis-aligned cubes is basically just
   * a(n affine) scaling in the coordinate directions.
   *
   * If dim < coorddim then local coordinates need to be suitably mapped to global ones.
   * AxisAlignedCubeGeometry uses a special std::bitset 'axes' for this.  'axes' has coorddim
   * entries, of which precisely 'dim' need to be set.  Each set entry marks a local
   * coordinate, i.e., a coordinate in which the cube has extension.  The cube is flat
   * in all other directions.  Its coordinates in these directions is taking from
   * the array called 'lower', which specifies the lower left corner of the hypercube.
   *
   * In the case of dim==coorddim, the code goes into overdrive.  Then special code path's
   * are taken (statically) which omit the conditionals needed to sort out the embedding
   * of local into global coordinates.  Aggressive compiler/scheduler optimization becomes
   * possible.  Additionally, the types returned by the methods jacobianTransposed
   * and jacobianInverseTransposed are dedicated types for diagonal matrices (DiagonalMatrix).
   *
   * \tparam CoordType Type used for single coordinate coefficients
   * \tparam dim Dimension of the cube
   * \tparam coorddim Dimension of the space that the cube lives in
   */
  template <class CoordType, unsigned int dim, unsigned int coorddim>
  class AxisAlignedCubeGeometry
  {


  public:

    /** \brief Dimension of the cube element */
    enum {mydimension = dim};

    /** \brief Dimension of the world space that the cube element is embedded in*/
    enum {coorddimension = coorddim};

    /** \brief Type used for single coordinate coefficients */
    typedef CoordType ctype;

    /** \brief Type used for a vector of element coordinates */
    typedef FieldVector<ctype,dim> LocalCoordinate;

    /** \brief Type used for a vector of world coordinates */
    typedef FieldVector<ctype,coorddim> GlobalCoordinate;

    /** \brief Return type of jacobianTransposed

        This is a fast DiagonalMatrix if dim==coorddim, and a FieldMatrix otherwise.
        The FieldMatrix will never contain more than one entry per row,
        hence it could be replaced by something more efficient.
     */
    typedef typename SelectType<dim==coorddim,
        DiagonalMatrix<ctype,dim>,
        FieldMatrix<ctype,dim,coorddim> >::Type JacobianTransposed;

    /** \brief Return type of jacobianInverseTransposed

        This is a fast DiagonalMatrix if dim==coorddim, and a FieldMatrix otherwise.
        The FieldMatrix will never contain more than one entry per column,
        hence it could be replaced by something more efficient.
     */
    typedef typename SelectType<dim==coorddim,
        DiagonalMatrix<ctype,dim>,
        FieldMatrix<ctype,coorddim,dim> >::Type JacobianInverseTransposed;

    /** \brief Constructor from a lower left and an upper right corner

        \note Only for dim==coorddim
     */
    AxisAlignedCubeGeometry(const Dune::FieldVector<ctype,coorddim> lower,
                            const Dune::FieldVector<ctype,coorddim> upper)
      : lower_(lower),
        upper_(upper),
        axes_((1<<coorddim)-1),     // all 'true', but is never actually used
        jacobianTransposed_(0),
        jacobianInverseTransposed_(0)
    {}

    /** \brief Constructor from a lower left and an upper right corner
     *
     *  \param lower Coordinates for the lower left corner.
     *  \param upper Coordinates for the upper right corner.
     *  \param axes Each bit set to 'true' here corresponds to a local coordinate axis.
     *         In other words, precisely 'dim' bits must be set here.
     */
    AxisAlignedCubeGeometry(const Dune::FieldVector<ctype,coorddim> lower,
                            const Dune::FieldVector<ctype,coorddim> upper,
                            const std::bitset<coorddim>& axes)
      : lower_(lower),
        upper_(upper),
        axes_(axes),
        jacobianTransposed_(0),
        jacobianInverseTransposed_(0)
    {
      assert(axes.count()==dim);
      for (size_t i=0; i<coorddim; i++)
        if (not axes_[i])
          upper_[i] = lower_[i];
    }

    /** \brief Copy constructor */
    AxisAlignedCubeGeometry& operator=(const AxisAlignedCubeGeometry& other)
    {
      lower_                     = other.lower_;
      upper_                     = other.upper_;
      axes_                      = other.axes_;
      jacobianTransposed_        = other.jacobianTransposed_;
      jacobianInverseTransposed_ = other.jacobianInverseTransposed_;
    }

    /** \brief Type of the cube.  Here: a hypercube of the correct dimension */
    GeometryType type() const
    {
      return GeometryType(GeometryType::cube,dim);
    }

    /** \brief Map a point in local (element) coordinates to world coordinates */
    GlobalCoordinate global(const LocalCoordinate& local) const
    {
      GlobalCoordinate result;
      if (dim == coorddim) {        // fast case
        for (size_t i=0; i<coorddim; i++)
          result[i] = lower_[i] + local[i]*(upper_[i] - lower_[i]);
      } else {          // slow case
        size_t lc=0;
        for (size_t i=0; i<coorddim; i++)
          result[i] = (axes_[i])
                      ? lower_[i] + local[lc++]*(upper_[i] - lower_[i])
                      : lower_[i];
      }
      return result;
    }

    /** \brief Map a point in global (world) coordinates to element coordinates */
    LocalCoordinate local(const GlobalCoordinate& global) const
    {
      LocalCoordinate result;
      if (dim == coorddim) {        // fast case
        for (size_t i=0; i<dim; i++)
          result[i] = (global[i] - lower_[i]) / (upper_[i] - lower_[i]);
      } else {          // slow case
        size_t lc=0;
        for (size_t i=0; i<coorddim; i++)
          if (axes_[i])
            result[lc++] = (global[i] - lower_[i]) / (upper_[i] - lower_[i]);
      }
      return result;
    }

    /** \brief Jacobian transposed of the transformation from local to global coordinates */
    const JacobianTransposed& jacobianTransposed(DUNE_UNUSED const LocalCoordinate& local) const
    {
      if (dim == coorddim) {         // fast case --> diagonal matrix
        for (size_t i=0; i<dim; i++)
          reinterpret_cast<DiagonalMatrix<ctype,dim>&>(jacobianTransposed_).diagonal()[i] = upper_[i] - lower_[i];
      } else {             // slow case --> dense matrix
        size_t lc = 0;
        for (size_t i=0; i<coorddim; i++)
          if (axes_[i])
            jacobianTransposed_[lc++][i] = upper_[i] - lower_[i];
      }

      return jacobianTransposed_;
    }

    /** \brief Jacobian transposed of the transformation from local to global coordinates */
    const JacobianInverseTransposed& jacobianInverseTransposed(DUNE_UNUSED const LocalCoordinate& local) const
    {
      if (dim == coorddim) {         // fast case --> diagonal matrix
        for (size_t i=0; i<dim; i++)
          reinterpret_cast<DiagonalMatrix<ctype,dim>&>(jacobianInverseTransposed_).diagonal()[i] = 1.0 / (upper_[i] - lower_[i]);
      } else {          // slow case --> dense matrix
        size_t lc = 0;
        for (size_t i=0; i<coorddim; i++)
          if (axes_[i])
            jacobianInverseTransposed_[i][lc++] = 1.0 / (upper_[i] - lower_[i]);
      }

      return jacobianInverseTransposed_;
    }

    /** \brief Return the integration element, i.e., the determinant term in the integral
               transformation formula
     */
    ctype integrationElement(DUNE_UNUSED const LocalCoordinate& local) const
    {
      return volume();
    }

    /** \brief Return center of mass of the element */
    GlobalCoordinate center() const
    {
      GlobalCoordinate result;
      // Since lower_==upper_ for unused coordinates, this always does the right thing
      for (size_t i=0; i<coorddim; i++)
        result[i] = 0.5 * (lower_[i] + upper_[i]);
      return result;
    }

    /** \brief Return the number of corners of the element */
    std::size_t corners() const
    {
      return 1<<dim;
    }

    /** \brief Return world coordinates of the k-th corner of the element */
    GlobalCoordinate corner(int k) const
    {
      GlobalCoordinate result;
      if (dim == coorddim) {         // fast case
        for (size_t i=0; i<coorddim; i++)
          result[i] = (k & (1<<i)) ? upper_[i] : lower_[i];
      } else {                // slow case
        unsigned int mask = 1;

        for (size_t i=0; i<coorddim; i++) {
          if (not axes_[i])
            result[i] = lower_[i];
          else {
            result[i] = (k & mask) ? upper_[i] : lower_[i];
            mask = (mask<<1);
          }
        }
      }


      return result;
    }

    /** \brief Return the element volume */
    ctype volume() const
    {
      ctype vol = 1;
      if (dim == coorddim) {         // fast case
        for (size_t i=0; i<dim; i++)
          vol *= upper_[i] - lower_[i];
      } else {         // slow case
        for (size_t i=0; i<coorddim; i++)
          if (axes_[i])
            vol *= upper_[i] - lower_[i];
      }
      return vol;
    }

    /** \brief Return if the element is affine.  Here: yes */
    bool affine() const
    {
      return true;
    }

  private:

    Dune::FieldVector<ctype,coorddim> lower_;

    Dune::FieldVector<ctype,coorddim> upper_;

    std::bitset<coorddim> axes_;

    // Storage so method jacobianTransposed can return a const reference
    mutable JacobianTransposed jacobianTransposed_;

    // Storage so method jacobianInverseTransposed can return a const reference
    mutable JacobianInverseTransposed jacobianInverseTransposed_;

  };

} // namespace Dune
#endif