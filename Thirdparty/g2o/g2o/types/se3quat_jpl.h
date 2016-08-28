#ifndef SE3QUATJPL_H_
#define SE3QUATJPL_H_

#include "JPL.hpp"

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace g2o {
  using namespace Eigen;

  typedef Matrix<double, 6, 1> Vector6d;
  typedef Matrix<double, 7, 1> Vector7d;

  class SE3QuatJPL {
    public:
      EIGEN_MAKE_ALIGNED_OPERATOR_NEW;


    protected:

      Quaterniond _r;
      Vector3d _t;


    public:
      SE3QuatJPL(){
        _r.setIdentity();
        _t.setZero();
      }

      SE3QuatJPL(const Matrix3d& R, const Vector3d& t):_r(Quaterniond(R)),_t(t){
        normalizeRotation();
      }

      SE3QuatJPL(const Quaterniond& q, const Vector3d& t):_r(q),_t(t){
        normalizeRotation();
      }

      /**
       * templaized constructor which allows v to be an arbitrary Eigen Vector type, e.g., Vector6d or Map<Vector6d>
       */
      template <typename Derived>
        explicit SE3QuatJPL(const MatrixBase<Derived>& v)
        {
          assert((v.size() == 6 || v.size() == 7) && "Vector dimension does not match");
          if (v.size() == 6) {
            for (int i=0; i<3; i++){
              _t[i]=v[i];
              _r.coeffs()(i)=v[i+3];
            }
            _r.w() = 0.; // recover the positive w
            if (_r.norm()>1.){
              _r.normalize();
            } else {
              double w2=1.-_r.squaredNorm();
              _r.w()= (w2<0.) ? 0. : sqrt(w2);
            }
          }
          else if (v.size() == 7) {
            int idx = 0;
            for (int i=0; i<3; ++i, ++idx)
              _t(i) = v(idx);
            for (int i=0; i<4; ++i, ++idx)
              _r.coeffs()(i) = v(idx);
            normalizeRotation();
          }
        }

      inline const Vector3d& translation() const {return _t;}

      inline void setTranslation(const Vector3d& t_) {_t = t_;}

      inline const Quaterniond& rotation() const {return _r;}

      void setRotation(const Quaterniond& r_) {_r=r_;}

      inline SE3QuatJPL operator* (const SE3QuatJPL& tr2) const{
        SE3QuatJPL result(*this);
        result._t += _r*tr2._t;
        result._r*=tr2._r;
        result.normalizeRotation();
        return result;
      }

      inline SE3QuatJPL& operator*= (const SE3QuatJPL& tr2){
        _t+=_r*tr2._t;
        _r*=tr2._r;
        normalizeRotation();
        return *this;
      }

      inline Vector3d operator* (const Vector3d& v) const {
        return _t+_r*v;
      }

      inline SE3QuatJPL inverse() const{
        SE3QuatJPL ret;
        ret._r=_r.conjugate();
        ret._t=ret._r*(_t*-1.);
        return ret;
      }

      inline double operator [](int i) const {
        assert(i<7);
        if (i<3)
          return _t[i];
        return _r.coeffs()[i-3];
      }


      inline Vector7d toVector() const{
        Vector7d v;
        v[0]=_t(0);
        v[1]=_t(1);
        v[2]=_t(2);
        v[3]=_r.x();
        v[4]=_r.y();
        v[5]=_r.z();
        v[6]=_r.w();
        return v;
      }

      inline void fromVector(const Vector7d& v){
        _r=Quaterniond(v[6], v[3], v[4], v[5]);
        _t=Vector3d(v[0], v[1], v[2]);
      }

      inline Vector6d toMinimalVector() const{
        Vector6d v;
        v[0]=_t(0);
        v[1]=_t(1);
        v[2]=_t(2);
        v[3]=_r.x();
        v[4]=_r.y();
        v[5]=_r.z();
        return v;
      }

      inline void fromMinimalVector(const Vector6d& v){
        double w = 1.-v[3]*v[3]-v[4]*v[4]-v[5]*v[5];
        if (w>0){
          _r=Quaterniond(sqrt(w), v[3], v[4], v[5]);
        } else {
          _r=Quaterniond(0, -v[3], -v[4], -v[5]);
        }
        _t=Vector3d(v[0], v[1], v[2]);
      }



      Vector6d log() const {
        Vector6d res;
        Matrix3d _R = _r.toRotationMatrix();
        double d =  0.5*(_R(0,0)+_R(1,1)+_R(2,2)-1);
        Vector3d omega;
        Vector3d upsilon;


        Vector3d dR = deltaR(_R);
        Matrix3d V_inv;

        if (d>0.99999)
        {

          omega=0.5*dR;
          Matrix3d Omega = skew(omega);
          V_inv = Matrix3d::Identity()- 0.5*Omega + (1./12.)*(Omega*Omega);
        }
        else
        {
          double theta = acos(d);
          omega = theta/(2*sqrt(1-d*d))*dR;
          Matrix3d Omega = skew(omega);
          V_inv = ( Matrix3d::Identity() - 0.5*Omega
              + ( 1-theta/(2*tan(theta/2)))/(theta*theta)*(Omega*Omega) );
        }

        upsilon = V_inv*_t;
        for (int i=0; i<3;i++){
          res[i]=omega[i];
        }
        for (int i=0; i<3;i++){
          res[i+3]=upsilon[i];
        }

        return res;

      }

      Vector3d map(const Vector3d & xyz) const
      {
        return _r*xyz + _t;
      }


      static SE3QuatJPL exp(const Vector6d & update)
      {
        Vector3d omega;
        for (int i=0; i<3; i++)
          omega[i]=update[i];
        Vector3d upsilon;
        for (int i=0; i<3; i++)
          upsilon[i]=update[i+3];

        double theta = omega.norm();
        Matrix3d Omega = skew(omega);

        Matrix3d R;
        Matrix3d V;
        if (theta<0.00001)
        {
          //TODO: CHECK WHETHER THIS IS CORRECT!!!
          R = (Matrix3d::Identity() + Omega + Omega*Omega);

          V = R;
        }
        else
        {
          Matrix3d Omega2 = Omega*Omega;

          R = (Matrix3d::Identity()
              + sin(theta)/theta *Omega
              + (1-cos(theta))/(theta*theta)*Omega2);

          V = (Matrix3d::Identity()
              + (1-cos(theta))/(theta*theta)*Omega
              + (theta-sin(theta))/(pow(theta,3))*Omega2);
        }
        return SE3QuatJPL(Quaterniond(R),V*upsilon);
      }

      Matrix<double, 6, 6> adj() const
      {
        Matrix3d R = _r.toRotationMatrix();
        Matrix<double, 6, 6> res;
        res.block(0,0,3,3) = R;
        res.block(3,3,3,3) = R;
        res.block(3,0,3,3) = skew(_t)*R;
        res.block(0,3,3,3) = Matrix3d::Zero(3,3);
        return res;
      }

      Matrix<double,4,4> to_homogeneous_matrix() const
      {
        Matrix<double,4,4> homogeneous_matrix;
        homogeneous_matrix.setIdentity();
        homogeneous_matrix.block(0,0,3,3) = _r.toRotationMatrix();
        homogeneous_matrix.col(3).head(3) = translation();

        return homogeneous_matrix;
      }

      void normalizeRotation(){
        if (_r.w()<0){
          _r.coeffs() *= -1;
        }
        _r.normalize();
      }

      /**
       * cast SE3Quat into an Eigen::Isometry3d
       */
      operator Eigen::Isometry3d() const
      {
        Eigen::Isometry3d result = (Eigen::Isometry3d) rotation();
        result.translation() = translation();
        return result;
      }
  };

  inline std::ostream& operator <<(std::ostream& out_str, const SE3QuatJPL& se3)
  {
    out_str << se3.to_homogeneous_matrix()  << std::endl;
    return out_str;
  }

#endif //se3quat_jpl.h
