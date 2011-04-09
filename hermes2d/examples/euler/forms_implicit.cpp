#include "hermes2d.h"

// Numerical fluxes.
#include "numerical_flux.h"

// Utility functions for the Euler equations.
#include "../euler_util.h"

class EulerEquationsWeakFormImplicit : public WeakForm
{
public:
  // Constructor.
  EulerEquationsWeakFormImplicit(NumericalFlux* num_flux, double kappa, double rho_ext, 
                                 double v1_ext, double v2_ext, double pressure_ext, 
                                 std::string solid_wall_bottom_marker, 
                                 std::string solid_wall_top_marker, 
                                 std::string inlet_marker, std::string outlet_marker, 
                                 Solution* prev_density, Solution* prev_density_vel_x, 
                                 Solution* prev_density_vel_y, Solution* prev_energy, 
                                 bool preconditioning, int num_of_equations = 4) :
  WeakForm(num_of_equations, true), rho_ext(rho_ext), v1_ext(v1_ext), v2_ext(v2_ext), 
  pressure_ext(pressure_ext), energy_ext(QuantityCalculator::calc_energy(rho_ext, 
  rho_ext * v1_ext, rho_ext * v2_ext, pressure_ext, kappa)) {
    
    if(preconditioning) {
      add_matrix_form(new EulerEquationsPreconditioning(0));
      add_matrix_form(new EulerEquationsPreconditioning(1));
      add_matrix_form(new EulerEquationsPreconditioning(2));
      add_matrix_form(new EulerEquationsPreconditioning(3));
    }
    
    add_vector_form(new EulerEquationsLinearFormTime(0));
    add_vector_form(new EulerEquationsLinearFormTime(1));
    add_vector_form(new EulerEquationsLinearFormTime(2));
    add_vector_form(new EulerEquationsLinearFormTime(3));
    
    for(unsigned int vector_form_i = 0; vector_form_i < this->vfvol.size(); vector_form_i++) {
      vfvol.at(vector_form_i)->ext.push_back(prev_density);
      vfvol.at(vector_form_i)->ext.push_back(prev_density_vel_x);
      vfvol.at(vector_form_i)->ext.push_back(prev_density_vel_y);
      vfvol.at(vector_form_i)->ext.push_back(prev_energy);
    }
    
    add_vector_form(new EulerEquationsLinearFormDensity());
    add_vector_form(new EulerEquationsLinearFormDensityVelX(kappa));
    add_vector_form(new EulerEquationsLinearFormDensityVelY(kappa));
    add_vector_form(new EulerEquationsLinearFormDensityEnergy(kappa));

    add_vector_form_surf(new EulerEquationsLinearFormInterface(0, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormInterface(1, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormInterface(2, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormInterface(3, num_flux));

    add_vector_form_surf(new EulerEquationsLinearFormSolidWall(0, solid_wall_bottom_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormSolidWall(1, solid_wall_bottom_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormSolidWall(2, solid_wall_bottom_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormSolidWall(3, solid_wall_bottom_marker, num_flux));

    if(solid_wall_bottom_marker != solid_wall_top_marker) {
      add_vector_form_surf(new EulerEquationsLinearFormSolidWall(0, solid_wall_top_marker, num_flux));
      add_vector_form_surf(new EulerEquationsLinearFormSolidWall(1, solid_wall_top_marker, num_flux));
      add_vector_form_surf(new EulerEquationsLinearFormSolidWall(2, solid_wall_top_marker, num_flux));
      add_vector_form_surf(new EulerEquationsLinearFormSolidWall(3, solid_wall_top_marker, num_flux));
    }
    else
      warning("Are you sure that solid wall top and bottom markers should coincide?");

    add_vector_form_surf(new EulerEquationsLinearFormInlet(0, inlet_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormInlet(1, inlet_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormInlet(2, inlet_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormInlet(3, inlet_marker, num_flux));

    add_vector_form_surf(new EulerEquationsLinearFormOutlet(0, outlet_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormOutlet(1, outlet_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormOutlet(2, outlet_marker, num_flux));
    add_vector_form_surf(new EulerEquationsLinearFormOutlet(3, outlet_marker, num_flux));
  };

  void set_time_step(double tau) {
    this->tau = tau;
  }

  double get_tau() const {
    return tau;
  }

  // Destructor.
  ~EulerEquationsWeakFormImplicit() {};
protected:
  class EulerEquationsPreconditioning : public WeakForm::MatrixFormVol
  {
  public:
    EulerEquationsPreconditioning(int i) : WeakForm::MatrixFormVol(i, i) {}

    template<typename Real, typename Scalar>
    Scalar matrix_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *u, Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      return int_u_v<Real, Scalar>(n, wt, u, v);
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *u, Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return matrix_form<double, scalar>(n, wt, u_ext, u, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *u, Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return matrix_form<Ord, Ord>(n, wt, u_ext, u, v, e, ext);
    }
  };

  class EulerEquationsLinearFormDensity : public WeakForm::VectorFormVol
  {
  public:
    EulerEquationsLinearFormDensity() : WeakForm::VectorFormVol(0) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, Geom<Real> *e, 
                       ExtData<Scalar> *ext) const {
      Scalar result = 0;
      for (int i = 0; i < n; i++) {
        result += wt[i] * u_ext[1]->val[i] * v->dx[i];
        result += wt[i] * u_ext[2]->val[i] * v->dy[i];
      }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, Geom<double> *e, 
                 ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }
  };

  class EulerEquationsLinearFormDensityVelX : public WeakForm::VectorFormVol
  {
  public:
    EulerEquationsLinearFormDensityVelX(double kappa) 
         : WeakForm::VectorFormVol(1), kappa(kappa) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      Scalar result = 0;
      for (int i = 0; i < n; i++) {
        result += wt[i] * u_ext[0]->val[i] 
                        * A_1_1_0<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[0]->val[i] 
                        * A_2_1_0<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
        result += wt[i] * u_ext[1]->val[i] 
                        * A_1_1_1<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[1]->val[i] 
                        * A_2_1_1<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
        result += wt[i] * u_ext[2]->val[i] 
                        * A_1_1_2<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[2]->val[i] 
                        * A_2_1_2<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
        result += wt[i] * u_ext[3]->val[i] 
                        * A_1_1_3<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[3]->val[i] 
                        * A_2_1_3<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
      }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }
protected:
    template<typename Scalar>
    Scalar A_1_1_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - ((rho_v_x * rho_v_x) / (rho * rho)) + 0.5 * (kappa - 1.) * 
             ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) /   (rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_1_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return 2 * (rho_v_x / rho) - (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_1_1_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (kappa - 1.) * (rho_v_y / rho);;
    }

    template<typename Scalar>
    Scalar A_1_1_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return kappa - 1.;
    }

    template<typename Scalar>
    Scalar A_2_1_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - rho_v_x * rho_v_y / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_2_1_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return rho_v_y / rho;
    }

    template<typename Scalar>
    Scalar A_2_1_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return rho_v_x / rho;
    }

    template<typename Scalar>
    Scalar A_2_1_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return 0;
    }

    double kappa;
  };

  class EulerEquationsLinearFormDensityVelY : public WeakForm::VectorFormVol
  {
  public:
    EulerEquationsLinearFormDensityVelY(double kappa) 
         : WeakForm::VectorFormVol(2), kappa(kappa) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      Scalar result = 0;
      for (int i = 0; i < n; i++) {
        result += wt[i] * u_ext[0]->val[i] 
                        * A_1_2_0<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[0]->val[i] 
                        * A_2_2_0<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
        result += wt[i] * u_ext[1]->val[i] 
                        * A_1_2_1<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[1]->val[i] 
                        * A_2_2_1<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
        result += wt[i] * u_ext[2]->val[i] 
                        * A_1_2_2<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[2]->val[i] 
                        * A_2_2_2<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
        result += wt[i] * u_ext[3]->val[i] 
                        * A_1_2_3<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[3]->val[i] 
                        * A_2_2_3<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
      }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }
  protected:
    template<typename Scalar>
    Scalar A_1_2_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - rho_v_x * rho_v_y / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_1_2_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return rho_v_y / rho;
    }

    template<typename Scalar>
    Scalar A_1_2_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return rho_v_x / rho;
    }

    template<typename Scalar>
    Scalar A_1_2_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return 0;
    }

    template<typename Scalar>
    Scalar A_2_2_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - ((rho_v_y * rho_v_y) / (rho * rho)) + 0.5 * (kappa - 1.) 
                * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) /   (rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_2_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_2_2_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return 2 * (rho_v_y / rho) - (kappa - 1.) * (rho_v_y / rho);
    }

    template<typename Scalar>
    Scalar A_2_2_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return kappa - 1.;
    }

    double kappa;
  };

  class EulerEquationsLinearFormDensityEnergy : public WeakForm::VectorFormVol
  {
  public:
    EulerEquationsLinearFormDensityEnergy(double kappa) 
         : WeakForm::VectorFormVol(3), kappa(kappa) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      Scalar result = 0;
      for (int i = 0; i < n; i++) {
        result += wt[i] * u_ext[0]->val[i] 
                        * A_1_3_0<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                        * v->dx[i];
        result += wt[i] * u_ext[0]->val[i] 
                        * A_2_3_0<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                        * v->dy[i];
        result += wt[i] * u_ext[1]->val[i] 
                        * A_1_3_1<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                        * v->dx[i];
        result += wt[i] * u_ext[1]->val[i] 
                        * A_2_3_1<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
        result += wt[i] * u_ext[2]->val[i] 
                        * A_1_3_2<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[2]->val[i] 
                        * A_2_3_2<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                        * v->dy[i];
        result += wt[i] * u_ext[3]->val[i] 
                        * A_1_3_3<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dx[i];
        result += wt[i] * u_ext[3]->val[i] 
                        * A_2_3_3<Scalar>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                        * v->dy[i];
      }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }
protected:
    template<typename Scalar>
    Scalar A_1_3_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (rho_v_x * energy) / (rho * rho) - (rho_v_x / (rho * rho)) * (kappa - 1.) 
               * (energy - ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho))) + (rho_v_x / rho) 
               * (kappa - 1.) * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_3_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return (energy / rho) + (1 / rho) * (kappa - 1.) * ( energy - ((rho_v_x * rho_v_x 
             + rho_v_y * rho_v_y) / (2 * rho * rho))) 
             - (kappa - 1.) * ((rho_v_x * rho_v_x) / (rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_3_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (kappa - 1.) * (rho_v_x * rho_v_y) / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_1_3_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return rho_v_x / rho + (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_2_3_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (rho_v_y * energy) / (rho * rho) - (rho_v_y / (rho * rho)) * (kappa - 1.) 
               * (energy - ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho))) 
               + (rho_v_y / rho) * (kappa - 1.) * ((rho_v_x * rho_v_x 
               + rho_v_y * rho_v_y) / (2 * rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_3_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (kappa - 1.) * (rho_v_x * rho_v_y) / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_2_3_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return (energy / rho) + (1 / rho) * (kappa - 1.) * ( energy - ((rho_v_x * rho_v_x 
             + rho_v_y * rho_v_y) / (2 * rho * rho))) 
             - (kappa - 1.) * ((rho_v_y * rho_v_y) / (rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_3_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return rho_v_y / rho + (kappa - 1.) * (rho_v_y / rho);
    }

    double kappa;
  };

  class EulerEquationsLinearFormTime : public WeakForm::VectorFormVol
  {
  public:
    EulerEquationsLinearFormTime(int i) 
         : WeakForm::VectorFormVol(i), component_i(i) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      return int_u_v<Real, Scalar>(n, wt, ext->fn[component_i], v)
        -
      int_u_v<Real, Scalar>(n, wt, u_ext[component_i], v);
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, 
            Geom<Ord> *e, ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }

    // Member.
    int component_i;
  };

  class EulerEquationsLinearFormInterface : public WeakForm::VectorFormSurf
  {
  public:
    EulerEquationsLinearFormInterface(int i, NumericalFlux* num_flux) 
         : WeakForm::VectorFormSurf(i, H2D_DG_INNER_EDGE), element(i), num_flux(num_flux) {}

    double value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      double result = 0;
      double w_L[4], w_R[4];
      for (int i = 0; i < n; i++) {
        w_L[0] = u_ext[0]->get_val_central(i);
        w_R[0] = u_ext[0]->get_val_neighbor(i);
    
        w_L[1] = u_ext[1]->get_val_central(i);
        w_R[1] = u_ext[1]->get_val_neighbor(i);

        w_L[2] = u_ext[2]->get_val_central(i);
        w_R[2] = u_ext[2]->get_val_neighbor(i);

        w_L[3] = u_ext[3]->get_val_central(i);
        w_R[3] = u_ext[3]->get_val_neighbor(i);

        result -= wt[i] * v->val[i] 
                        * num_flux->numerical_flux_i(element, w_L, w_R, e->nx[i], e->ny[i]);
    }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    int element;
    NumericalFlux* num_flux;
  };

  class EulerEquationsLinearFormSolidWall : public WeakForm::VectorFormSurf
  {
  public:
    EulerEquationsLinearFormSolidWall(int i, std::string marker, NumericalFlux* num_flux) 
         : WeakForm::VectorFormSurf(i, marker), element(i), num_flux(num_flux) {}

    double value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      double result = 0;
      for (int i = 0; i < n; i++) {
        double w_L[4];
        w_L[0] = u_ext[0]->val[i];
        w_L[1] = u_ext[1]->val[i];
        w_L[2] = u_ext[2]->val[i];
        w_L[3] = u_ext[3]->val[i];

        result -= wt[i] * v->val[i] * num_flux->numerical_flux_solid_wall_i(element, 
                  w_L, e->nx[i], e->ny[i]);
      }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    int element;
    NumericalFlux* num_flux;
  };

  class EulerEquationsLinearFormInlet : public WeakForm::VectorFormSurf
  {
  public:
    EulerEquationsLinearFormInlet(int i, std::string marker, NumericalFlux* num_flux) 
         : WeakForm::VectorFormSurf(i, marker), element(i), num_flux(num_flux) {}

    double value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, Geom<double> *e, 
                 ExtData<scalar> *ext) const {
      double result = 0;
      double w_L[4], w_B[4];

      for (int i = 0; i < n; i++) {
        // Left (inner) state from the previous time level solution.
        w_L[0] = u_ext[0]->val[i];
        w_L[1] = u_ext[1]->val[i];
        w_L[2] = u_ext[2]->val[i];
        w_L[3] = u_ext[3]->val[i];
    
        w_B[0] = static_cast<EulerEquationsWeakFormImplicit*>(wf)->rho_ext;
        w_B[1] = static_cast<EulerEquationsWeakFormImplicit*>(wf)->rho_ext 
                 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->v1_ext;
        w_B[2] = static_cast<EulerEquationsWeakFormImplicit*>(wf)->rho_ext 
                 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->v2_ext;
        w_B[3] = static_cast<EulerEquationsWeakFormImplicit*>(wf)->energy_ext;
    
        result -= wt[i] * v->val[i] * num_flux->numerical_flux_inlet_i(element, 
                  w_L, w_B, e->nx[i], e->ny[i]);
      }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    int element;
    NumericalFlux* num_flux;
  };

  class EulerEquationsLinearFormOutlet : public WeakForm::VectorFormSurf
  {
  public:
    EulerEquationsLinearFormOutlet(int i, std::string marker, NumericalFlux* num_flux) : 
    WeakForm::VectorFormSurf(i, marker), element(i), num_flux(num_flux) {}

    double value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      double result = 0;
      double w_L[4];
      for (int i = 0; i < n; i++) {
        w_L[0] = u_ext[0]->val[i];
        w_L[1] = u_ext[1]->val[i];
        w_L[2] = u_ext[2]->val[i];
        w_L[3] = u_ext[3]->val[i];
        result -= wt[i] * v->val[i] 
                        * num_flux->numerical_flux_outlet_i(element, w_L, 
                          static_cast<EulerEquationsWeakFormImplicit*>(wf)->pressure_ext, 
                          e->nx[i], e->ny[i]);
      }
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    int element;
    NumericalFlux* num_flux;
  };
  // Members.
  double rho_ext;
  double v1_ext;
  double v2_ext;
  double pressure_ext;
  double energy_ext;
  double tau;
};

class EulerEquationsWeakFormImplicitMultiComponent : public WeakForm
{
public:
  // Constructor.
  EulerEquationsWeakFormImplicitMultiComponent(NumericalFlux* num_flux, double kappa, 
        double rho_ext, double v1_ext, double v2_ext, double pressure_ext, 
        std::string solid_wall_bottom_marker, std::string solid_wall_top_marker, 
        std::string inlet_marker, std::string outlet_marker, 
        Solution* prev_density, Solution* prev_density_vel_x, Solution* prev_density_vel_y, 
        Solution* prev_energy, bool preconditioning, int num_of_equations = 4) :
        WeakForm(num_of_equations, true), rho_ext(rho_ext), v1_ext(v1_ext), v2_ext(v2_ext), 
        pressure_ext(pressure_ext), energy_ext(QuantityCalculator::calc_energy(rho_ext, 
        rho_ext * v1_ext, rho_ext * v2_ext, pressure_ext, kappa)) 
  {
    Hermes::vector<std::pair<unsigned int, unsigned int> > matrix_coordinates;
    matrix_coordinates.push_back(std::pair<unsigned int, unsigned int>(0, 0));
    matrix_coordinates.push_back(std::pair<unsigned int, unsigned int>(1, 1));
    matrix_coordinates.push_back(std::pair<unsigned int, unsigned int>(2, 2));
    matrix_coordinates.push_back(std::pair<unsigned int, unsigned int>(3, 3));

    Hermes::vector<std::pair<unsigned int, unsigned int> > matrix_coordinates_precon_vol;
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(0, 0));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(0, 1));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(0, 2));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(0, 3));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(1, 0));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(1, 1));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(1, 2));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(1, 3));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(2, 0));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(2, 1));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(2, 2));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(2, 3));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(3, 0));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(3, 1));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(3, 2));
    matrix_coordinates_precon_vol.push_back(std::pair<unsigned int, unsigned int>(3, 3));

    Hermes::vector<unsigned int> vector_coordinates;
    vector_coordinates.push_back(0);
    vector_coordinates.push_back(1);
    vector_coordinates.push_back(2);
    vector_coordinates.push_back(3);

    if(preconditioning) {
      
      //EulerEquationsMatrixFormVolPreconditioning* precon_vol 
      //  = new EulerEquationsMatrixFormVolPreconditioning(matrix_coordinates_precon_vol, kappa);
      //EulerEquationsMatrixFormSurfPreconditioning* precon_surf 
      // = new EulerEquationsMatrixFormSurfPreconditioning(matrix_coordinates, kappa);
      //add_multicomponent_matrix_form(precon_vol);
      //add_multicomponent_matrix_form_surf(precon_surf);
      
      add_multicomponent_matrix_form(new EulerEquationsMatrixFormVolPreconditioningSimple(matrix_coordinates));
    }

    add_multicomponent_vector_form(new EulerEquationsLinearFormTime(vector_coordinates));

    for(unsigned int vector_form_i = 0; vector_form_i < this->vfvol_mc.size(); vector_form_i++) {
      vfvol_mc.at(vector_form_i)->ext.push_back(prev_density);
      vfvol_mc.at(vector_form_i)->ext.push_back(prev_density_vel_x);
      vfvol_mc.at(vector_form_i)->ext.push_back(prev_density_vel_y);
      vfvol_mc.at(vector_form_i)->ext.push_back(prev_energy);
    }

    add_multicomponent_vector_form(new EulerEquationsLinearForm(vector_coordinates, kappa));
    
    add_multicomponent_vector_form_surf(new EulerEquationsLinearFormInterface(vector_coordinates, num_flux));

    add_multicomponent_vector_form_surf(new EulerEquationsLinearFormSolidWall(vector_coordinates, 
                                        solid_wall_bottom_marker, num_flux));
    if(solid_wall_bottom_marker != solid_wall_top_marker)
      add_multicomponent_vector_form_surf(new EulerEquationsLinearFormSolidWall(vector_coordinates, 
                                          solid_wall_top_marker, num_flux));

    add_multicomponent_vector_form_surf(new EulerEquationsLinearFormInlet(vector_coordinates, 
                                        inlet_marker, num_flux));

    add_multicomponent_vector_form_surf(new EulerEquationsLinearFormOutlet(vector_coordinates, 
                                        outlet_marker, num_flux));
  };

  void set_time_step(double tau) {
    this->tau = tau;
  }

  double get_tau() const {
    return tau;
  }

  // Destructor.
  ~EulerEquationsWeakFormImplicitMultiComponent() {};
protected:
  class EulerEquationsMatrixFormVolPreconditioning : public WeakForm::MultiComponentMatrixFormVol
  {
  public:
    EulerEquationsMatrixFormVolPreconditioning(Hermes::vector<std::pair<unsigned int, 
                                               unsigned int> >coordinates, double kappa) 
         : WeakForm::MultiComponentMatrixFormVol(coordinates), kappa(kappa) {}

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *u, Func<double> *v, 
               Geom<double> *e, ExtData<double> *ext, Hermes::vector<double>& result) const {
      double result_0_0 = 0;
      double result_0_1 = 0;
      double result_0_2 = 0;
      double result_0_3 = 0;

      double result_1_0 = 0;
      double result_1_1 = 0;
      double result_1_2 = 0;
      double result_1_3 = 0;

      double result_2_0 = 0;
      double result_2_1 = 0;
      double result_2_2 = 0;
      double result_2_3 = 0;

      double result_3_0 = 0;
      double result_3_1 = 0;
      double result_3_2 = 0;
      double result_3_3 = 0;

      for (int i = 0; i < n; i++) {
        result_0_0 += wt[i] * u->val[i] * v->val[i] 
                            / static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
        result_0_1 += wt[i] * u->val[i] * v->dx[i];
        result_0_2 += wt[i] * u->val[i] * v->dy[i];
        
        result_1_0 += wt[i] * u->val[i] 
                            * A_1_1_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_1_0 += wt[i] * u->val[i] 
                            * A_2_1_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
        result_1_1 += wt[i] * u->val[i] 
                            * v->val[i] / static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
        result_1_1 += wt[i] * u->val[i] 
                            * A_1_1_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_1_1 += wt[i] * u->val[i] 
                            * A_2_1_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
        result_1_2 += wt[i] * u->val[i] 
                            * A_1_1_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_1_2 += wt[i] * u->val[i] 
                            * A_2_1_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
        result_1_3 += wt[i] * u->val[i] 
                            * A_1_1_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_1_3 += wt[i] * u->val[i] 
                            * A_2_1_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
      
        result_2_0 += wt[i] * u->val[i] 
                            * A_1_2_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_2_0 += wt[i] * u->val[i] 
                            * A_2_2_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
        result_2_1 += wt[i] * u->val[i] 
                            * A_1_2_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_2_1 += wt[i] * u->val[i] 
                            * A_2_2_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
        result_2_2 += wt[i] * u->val[i] 
                            * v->val[i] 
                            / static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
        result_2_2 += wt[i] * u->val[i] 
                            * A_1_2_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_2_2 += wt[i] * u->val[i] 
                            * A_2_2_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
        result_2_3 += wt[i] * u->val[i] 
                            * A_1_2_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_2_3 += wt[i] * u->val[i] 
                            * A_2_2_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
     
        result_3_0 += wt[i] * u->val[i] 
                            * A_1_3_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                            * v->dx[i];
        result_3_0 += wt[i] * u->val[i] 
                            * A_2_3_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                            * v->dy[i];
        result_3_1 += wt[i] * u->val[i] 
                            * A_1_3_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                            * v->dx[i];
        result_3_1 += wt[i] * u->val[i] 
                            * A_2_3_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
        result_3_2 += wt[i] * u->val[i] 
                            * A_1_3_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_3_2 += wt[i] * u->val[i] 
                            * A_2_3_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                            * v->dy[i];
        result_3_3 += wt[i] * u->val[i] 
                            * v->val[i] / static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
        result_3_3 += wt[i] * u->val[i] 
                            * A_1_3_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dx[i];
        result_3_3 += wt[i] * u->val[i] 
                            * A_2_3_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                            * v->dy[i];
      }
      result.push_back(result_0_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_0_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_0_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_0_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());

      result.push_back(result_1_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());

      result.push_back(result_2_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());

      result.push_back(result_3_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *u, Func<Ord> *v, 
            Geom<Ord> *e, ExtData<Ord> *ext) const {
      return int_u_v<Ord, Ord>(n, wt, u, v);
    }

    protected:
    template<typename Scalar>
    Scalar A_1_1_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - ((rho_v_x * rho_v_x) / (rho * rho)) + 0.5 * (kappa - 1.) * 
             ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) /   (rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_1_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 2 * (rho_v_x / rho) - (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_1_1_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_y / rho);;
    }

    template<typename Scalar>
    Scalar A_1_1_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return kappa - 1.;
    }

    template<typename Scalar>
    Scalar A_2_1_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - rho_v_x * rho_v_y / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_2_1_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_y / rho;
    }

    template<typename Scalar>
    Scalar A_2_1_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_x / rho;
    }

    template<typename Scalar>
    Scalar A_2_1_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 0;
    }

    template<typename Scalar>
    Scalar A_1_2_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - rho_v_x * rho_v_y / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_1_2_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_y / rho;
    }

    template<typename Scalar>
    Scalar A_1_2_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_x / rho;
    }

    template<typename Scalar>
    Scalar A_1_2_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 0;
    }

    template<typename Scalar>
    Scalar A_2_2_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - ((rho_v_y * rho_v_y) / (rho * rho)) + 0.5 * (kappa - 1.) * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) /   (rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_2_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_2_2_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 2 * (rho_v_y / rho) - (kappa - 1.) * (rho_v_y / rho);
    }

    template<typename Scalar>
    Scalar A_2_2_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return kappa - 1.;
    }

    template<typename Scalar>
    Scalar A_1_3_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (rho_v_x * energy) / (rho * rho) - (rho_v_x / (rho * rho)) * (kappa - 1.) 
               * (energy - ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho))) 
               + (rho_v_x / rho) * (kappa - 1.) * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) 
               / (2 * rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_3_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return (energy / rho) + (1 / rho) * (kappa - 1.) * ( energy 
              - ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho * rho))) 
              - (kappa - 1.) * ((rho_v_x * rho_v_x) / (rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_3_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_x * rho_v_y) / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_1_3_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_x / rho + (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_2_3_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (rho_v_y * energy) / (rho * rho) - (rho_v_y / (rho * rho)) * (kappa - 1.) 
               * (energy - ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho))) 
               + (rho_v_y / rho) * (kappa - 1.) * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) 
               / (2 * rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_3_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_x * rho_v_y) / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_2_3_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return (energy / rho) + (1 / rho) * (kappa - 1.) * ( energy - ((rho_v_x * rho_v_x 
             + rho_v_y * rho_v_y) / (2 * rho * rho))) - (kappa - 1.) * ((rho_v_y * rho_v_y) 
             / (rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_3_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_y / rho + (kappa - 1.) * (rho_v_y / rho);
    }

    double kappa;
  };

  class EulerEquationsMatrixFormSurfPreconditioning : public WeakForm::MultiComponentMatrixFormSurf
  {
  public:
    EulerEquationsMatrixFormSurfPreconditioning(Hermes::vector<std::pair<unsigned int, 
                                                unsigned int> >coordinates, double kappa) 
    : WeakForm::MultiComponentMatrixFormSurf(coordinates, H2D_DG_INNER_EDGE), 
      num_flux(new StegerWarmingNumericalFlux(kappa)) { }

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *u, 
               Func<double> *v, Geom<double> *e, ExtData<double> *ext, 
               Hermes::vector<double>& result) const {
      double result_0_0 = 0;
      double result_0_1 = 0;
      double result_0_2 = 0;
      double result_0_3 = 0;

      double result_1_0 = 0;
      double result_1_1 = 0;
      double result_1_2 = 0;
      double result_1_3 = 0;

      double result_2_0 = 0;
      double result_2_1 = 0;
      double result_2_2 = 0;
      double result_2_3 = 0;

      double result_3_0 = 0;
      double result_3_1 = 0;
      double result_3_2 = 0;
      double result_3_3 = 0;

      double result_L[4];
      double result_R[4];

      double w_L[4], w_R[4];

      for (int i = 0; i < n; i++) {
        w_L[0] = u_ext[0]->get_val_central(i);
        w_R[0] = u_ext[0]->get_val_neighbor(i);
    
        w_L[1] = u_ext[1]->get_val_central(i);
        w_R[1] = u_ext[1]->get_val_neighbor(i);

        w_L[2] = u_ext[2]->get_val_central(i);
        w_R[2] = u_ext[2]->get_val_neighbor(i);

        w_L[3] = u_ext[3]->get_val_central(i);
        w_R[3] = u_ext[3]->get_val_neighbor(i);

        double P_plus_1[4] = {1, 0, 0, 0};
        double P_plus_2[4] = {0, 1, 0, 0};
        double P_plus_3[4] = {0, 0, 1, 0};
        double P_plus_4[4] = {0, 0, 0, 1};
      
        double P_minus_1[4] = {1, 0, 0, 0};
        double P_minus_2[4] = {0, 1, 0, 0};
        double P_minus_3[4] = {0, 0, 1, 0};
        double P_minus_4[4] = {0, 0, 0, 1};

        num_flux->P_plus(P_plus_1, w_L, P_plus_1, e->nx[i], e->ny[i]);
        num_flux->P_plus(P_plus_2, w_L, P_plus_2, e->nx[i], e->ny[i]);
        num_flux->P_plus(P_plus_3, w_L, P_plus_3, e->nx[i], e->ny[i]);
        num_flux->P_plus(P_plus_4, w_L, P_plus_4, e->nx[i], e->ny[i]);
      
        num_flux->P_minus(P_minus_1, w_R, P_minus_1, e->nx[i], e->ny[i]);
        num_flux->P_minus(P_minus_2, w_R, P_minus_2, e->nx[i], e->ny[i]);
        num_flux->P_minus(P_minus_3, w_R, P_minus_3, e->nx[i], e->ny[i]);
        num_flux->P_minus(P_minus_4, w_R, P_minus_4, e->nx[i], e->ny[i]);

        result_0_0 += wt[i] * (P_plus_1[0] * u->get_val_central(i) + P_minus_1[0] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_0_1 += wt[i] * (P_plus_1[1] * u->get_val_central(i) + P_minus_1[1] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_0_2 += wt[i] * (P_plus_1[2] * u->get_val_central(i) + P_minus_1[2] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_0_3 += wt[i] * (P_plus_1[3] * u->get_val_central(i) + P_minus_1[3] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));

        result_1_0 += wt[i] * (P_plus_2[0] * u->get_val_central(i) + P_minus_2[0] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_1_1 += wt[i] * (P_plus_2[1] * u->get_val_central(i) + P_minus_2[1] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_1_2 += wt[i] * (P_plus_2[2] * u->get_val_central(i) + P_minus_2[2] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_1_3 += wt[i] * (P_plus_2[3] * u->get_val_central(i) + P_minus_2[3] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));

        result_2_0 += wt[i] * (P_plus_3[0] * u->get_val_central(i) + P_minus_3[0] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_2_1 += wt[i] * (P_plus_3[1] * u->get_val_central(i) + P_minus_3[1] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_2_2 += wt[i] * (P_plus_3[2] * u->get_val_central(i) + P_minus_3[2] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_2_3 += wt[i] * (P_plus_3[3] * u->get_val_central(i) + P_minus_3[3] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));

        result_3_0 += wt[i] * (P_plus_4[0] * u->get_val_central(i) + P_minus_4[0] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_3_1 += wt[i] * (P_plus_4[1] * u->get_val_central(i) + P_minus_4[1] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_3_2 += wt[i] * (P_plus_4[2] * u->get_val_central(i) + P_minus_4[2] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
        result_3_3 += wt[i] * (P_plus_4[3] * u->get_val_central(i) + P_minus_4[3] 
                            * u->get_val_neighbor(i)) * (v->get_val_central(i) - v->get_val_neighbor(i));
      }

      result.push_back(result_0_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_0_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_0_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_0_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());

      result.push_back(result_1_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());

      result.push_back(result_2_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());

      result.push_back(result_3_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *u, Func<Ord> *v, 
            Geom<Ord> *e, ExtData<Ord> *ext) const {
      Ord result = 0;
      for (int i = 0; i < n; i++) {
        result += wt[i] * u->get_val_central(i) * v->get_val_central(i);
        result += wt[i] * u->get_val_neighbor(i) * v->get_val_neighbor(i);
      }
      return result;
    }

    StegerWarmingNumericalFlux* num_flux;
  };

  class EulerEquationsMatrixFormVolPreconditioningSimple 
             : public WeakForm::MultiComponentMatrixFormVol
  {
  public:
    EulerEquationsMatrixFormVolPreconditioningSimple(Hermes::vector<std::pair<unsigned int, 
                                                     unsigned int> >coordinates) 
         : WeakForm::MultiComponentMatrixFormVol(coordinates) {}

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *u, Func<double> *v, 
               Geom<double> *e, ExtData<double> *ext, Hermes::vector<double>& result) const {
      double result_n = int_u_v<double, double>(n, wt, u, v);
      result.push_back(result_n);
      result.push_back(result_n);
      result.push_back(result_n);
      result.push_back(result_n);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *u, Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return int_u_v<Ord, Ord>(n, wt, u, v);
    }
  };

  class EulerEquationsLinearForm : public WeakForm::MultiComponentVectorFormVol
  {
  public:
    EulerEquationsLinearForm(Hermes::vector<unsigned int> coordinates, double kappa) 
                             : WeakForm::MultiComponentVectorFormVol(coordinates), kappa(kappa) { }

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *v, Geom<double> *e, 
               ExtData<double> *ext, Hermes::vector<double>& result) const {
      double result_0 = 0;
      double result_1 = 0;
      double result_2 = 0;
      double result_3 = 0;
      for (int i = 0; i < n; i++) {
        result_0 += wt[i] * u_ext[1]->val[i] * v->dx[i];
        result_0 += wt[i] * u_ext[2]->val[i] * v->dy[i];
        
        result_1 += wt[i] * u_ext[0]->val[i] 
                          * A_1_1_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_1 += wt[i] * u_ext[0]->val[i] 
                          * A_2_1_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
        result_1 += wt[i] * u_ext[1]->val[i] 
                          * A_1_1_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_1 += wt[i] * u_ext[1]->val[i] 
                          * A_2_1_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
        result_1 += wt[i] * u_ext[2]->val[i] 
                          * A_1_1_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_1 += wt[i] * u_ext[2]->val[i] 
                          * A_2_1_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
        result_1 += wt[i] * u_ext[3]->val[i] 
                          * A_1_1_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_1 += wt[i] * u_ext[3]->val[i] 
                          * A_2_1_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
      
        result_2 += wt[i] * u_ext[0]->val[i] 
                          * A_1_2_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_2 += wt[i] * u_ext[0]->val[i] 
                          * A_2_2_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
        result_2 += wt[i] * u_ext[1]->val[i] 
                          * A_1_2_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_2 += wt[i] * u_ext[1]->val[i] 
                          * A_2_2_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
        result_2 += wt[i] * u_ext[2]->val[i] 
                          * A_1_2_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_2 += wt[i] * u_ext[2]->val[i] 
                          * A_2_2_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
        result_2 += wt[i] * u_ext[3]->val[i] 
                          * A_1_2_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_2 += wt[i] * u_ext[3]->val[i] 
                          * A_2_2_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
     
        result_3 += wt[i] * u_ext[0]->val[i] 
                          * A_1_3_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                          * v->dx[i];
        result_3 += wt[i] * u_ext[0]->val[i] 
                          * A_2_3_0<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                          * v->dy[i];
        result_3 += wt[i] * u_ext[1]->val[i] 
                          * A_1_3_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                          * v->dx[i];
        result_3 += wt[i] * u_ext[1]->val[i] 
                          * A_2_3_1<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
        result_3 += wt[i] * u_ext[2]->val[i] 
                          * A_1_3_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_3 += wt[i] * u_ext[2]->val[i] 
                          * A_2_3_2<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], u_ext[3]->val[i]) 
                          * v->dy[i];
        result_3 += wt[i] * u_ext[3]->val[i] 
                          * A_1_3_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dx[i];
        result_3 += wt[i] * u_ext[3]->val[i] 
                          * A_2_3_3<double>(u_ext[0]->val[i], u_ext[1]->val[i], u_ext[2]->val[i], 0) 
                          * v->dy[i];
      }
      result.push_back(result_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, ExtData<Ord> *ext) const {
      return v->val[0] * v->val[0] * v->val[0] * v->val[0];
    }
  protected:
    template<typename Scalar>
    Scalar A_1_1_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - ((rho_v_x * rho_v_x) / (rho * rho)) + 0.5 * (kappa - 1.) * 
             ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) /   (rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_1_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 2 * (rho_v_x / rho) - (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_1_1_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_y / rho);;
    }

    template<typename Scalar>
    Scalar A_1_1_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return kappa - 1.;
    }

    template<typename Scalar>
    Scalar A_2_1_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - rho_v_x * rho_v_y / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_2_1_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_y / rho;
    }

    template<typename Scalar>
    Scalar A_2_1_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_x / rho;
    }

    template<typename Scalar>
    Scalar A_2_1_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 0;
    }

    template<typename Scalar>
    Scalar A_1_2_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - rho_v_x * rho_v_y / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_1_2_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_y / rho;
    }

    template<typename Scalar>
    Scalar A_1_2_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_x / rho;
    }

    template<typename Scalar>
    Scalar A_1_2_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 0;
    }

    template<typename Scalar>
    Scalar A_2_2_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - ((rho_v_y * rho_v_y) / (rho * rho)) + 0.5 * (kappa - 1.) 
             * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) /   (rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_2_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_2_2_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return 2 * (rho_v_y / rho) - (kappa - 1.) * (rho_v_y / rho);
    }

    template<typename Scalar>
    Scalar A_2_2_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return kappa - 1.;
    }

    template<typename Scalar>
    Scalar A_1_3_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const {
      return - (rho_v_x * energy) / (rho * rho) - (rho_v_x / (rho * rho)) * (kappa - 1.) 
             * (energy - ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho))) + (rho_v_x / rho) 
             * (kappa - 1.) * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_3_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return (energy / rho) + (1 / rho) * (kappa - 1.) * ( energy - ((rho_v_x * rho_v_x 
             + rho_v_y * rho_v_y) / (2 * rho * rho))) - (kappa - 1.) * ((rho_v_x * rho_v_x) 
             / (rho * rho));
    }

    template<typename Scalar>
    Scalar A_1_3_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_x * rho_v_y) / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_1_3_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_x / rho + (kappa - 1.) * (rho_v_x / rho);
    }

    template<typename Scalar>
    Scalar A_2_3_0(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (rho_v_y * energy) / (rho * rho) - (rho_v_y / (rho * rho)) * (kappa - 1.) 
             * (energy - ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho))) + (rho_v_y / rho) 
             * (kappa - 1.) * ((rho_v_x * rho_v_x + rho_v_y * rho_v_y) / (2 * rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_3_1(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return - (kappa - 1.) * (rho_v_x * rho_v_y) / (rho * rho);
    }

    template<typename Scalar>
    Scalar A_2_3_2(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return (energy / rho) + (1 / rho) * (kappa - 1.) * ( energy - ((rho_v_x * rho_v_x 
             + rho_v_y * rho_v_y) / (2 * rho * rho))) - (kappa - 1.) * ((rho_v_y * rho_v_y) / (rho * rho));
    }

    template<typename Scalar>
    Scalar A_2_3_3(Scalar rho, Scalar rho_v_x, Scalar rho_v_y, Scalar energy) const{
      return rho_v_y / rho + (kappa - 1.) * (rho_v_y / rho);
    }

    double kappa;
  };
  
  class EulerEquationsLinearFormTime : public WeakForm::MultiComponentVectorFormVol
  {
  public:
    EulerEquationsLinearFormTime(Hermes::vector<unsigned int> coordinates) 
         : WeakForm::MultiComponentVectorFormVol(coordinates) {}

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *v, Geom<double> *e, 
               ExtData<double> *ext, Hermes::vector<double>& result) const {
      result.push_back(int_u_v<double, double>(n, wt, ext->fn[0], v) 
                       - int_u_v<double, double>(n, wt, u_ext[0], v));
      result.push_back(int_u_v<double, double>(n, wt, ext->fn[1], v) 
                       - int_u_v<double, double>(n, wt, u_ext[1], v));
      result.push_back(int_u_v<double, double>(n, wt, ext->fn[2], v) 
                       - int_u_v<double, double>(n, wt, u_ext[2], v));
      result.push_back(int_u_v<double, double>(n, wt, ext->fn[3], v) 
                       - int_u_v<double, double>(n, wt, u_ext[3], v));
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return int_u_v<Ord, Ord>(n, wt, v, v);
    }
  };

  class EulerEquationsLinearFormInterface : public WeakForm::MultiComponentVectorFormSurf
  {
  public:
    EulerEquationsLinearFormInterface(Hermes::vector<unsigned int> coordinates, 
                                      NumericalFlux* num_flux) 
         : WeakForm::MultiComponentVectorFormSurf(coordinates, H2D_DG_INNER_EDGE), num_flux(num_flux) {}

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *v, 
               Geom<double> *e, ExtData<double> *ext, Hermes::vector<double>& result) const {
      double result_0 = 0;
      double result_1 = 0;
      double result_2 = 0;
      double result_3 = 0;
      double w_L[4], w_R[4];
      for (int i = 0; i < n; i++) {
        w_L[0] = u_ext[0]->get_val_central(i);
        w_R[0] = u_ext[0]->get_val_neighbor(i);
    
        w_L[1] = u_ext[1]->get_val_central(i);
        w_R[1] = u_ext[1]->get_val_neighbor(i);

        w_L[2] = u_ext[2]->get_val_central(i);
        w_R[2] = u_ext[2]->get_val_neighbor(i);

        w_L[3] = u_ext[3]->get_val_central(i);
        w_R[3] = u_ext[3]->get_val_neighbor(i);

        double flux[4];
        num_flux->numerical_flux(flux, w_L, w_R, e->nx[i], e->ny[i]);

        result_0 -= wt[i] * v->val[i] * flux[0];
        result_1 -= wt[i] * v->val[i] * flux[1];
        result_2 -= wt[i] * v->val[i] * flux[2];
        result_3 -= wt[i] * v->val[i] * flux[3];
      }
      result.push_back(result_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    NumericalFlux* num_flux;
  };

  class EulerEquationsLinearFormSolidWall : public WeakForm::MultiComponentVectorFormSurf
  {
  public:
    EulerEquationsLinearFormSolidWall(Hermes::vector<unsigned int> coordinates, 
                                      std::string marker, NumericalFlux* num_flux) 
         : WeakForm::MultiComponentVectorFormSurf(coordinates, marker), num_flux(num_flux) {}

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *v, Geom<double> *e, 
               ExtData<double> *ext, Hermes::vector<double>& result) const {
      double result_0 = 0;
      double result_1 = 0;
      double result_2 = 0;
      double result_3 = 0;
      for (int i = 0; i < n; i++) {
        double w_L[4];
        w_L[0] = u_ext[0]->val[i];
        w_L[1] = u_ext[1]->val[i];
        w_L[2] = u_ext[2]->val[i];
        w_L[3] = u_ext[3]->val[i];

        double flux[4];
        num_flux->numerical_flux_solid_wall(flux, w_L, e->nx[i], e->ny[i]);

        result_0 -= wt[i] * v->val[i] * flux[0];
        result_1 -= wt[i] * v->val[i] * flux[1];
        result_2 -= wt[i] * v->val[i] * flux[2];
        result_3 -= wt[i] * v->val[i] * flux[3];
      }
      result.push_back(result_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    NumericalFlux* num_flux;
  };

  class EulerEquationsLinearFormInlet : public WeakForm::MultiComponentVectorFormSurf
  {
  public:
    EulerEquationsLinearFormInlet(Hermes::vector<unsigned int> coordinates, 
                                  std::string marker, NumericalFlux* num_flux) : 
        WeakForm::MultiComponentVectorFormSurf(coordinates, marker), num_flux(num_flux) {}

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *v, Geom<double> *e, 
               ExtData<double> *ext, Hermes::vector<double>& result) const {
      double result_0 = 0;
      double result_1 = 0;
      double result_2 = 0;
      double result_3 = 0;
      double w_L[4], w_B[4];

      for (int i = 0; i < n; i++) {
        // Left (inner) state from the previous time level solution.
        w_L[0] = u_ext[0]->val[i];
        w_L[1] = u_ext[1]->val[i];
        w_L[2] = u_ext[2]->val[i];
        w_L[3] = u_ext[3]->val[i];
    
        w_B[0] = static_cast<EulerEquationsWeakFormImplicitMultiComponent*>(wf)->rho_ext;
        w_B[1] = static_cast<EulerEquationsWeakFormImplicitMultiComponent*>(wf)->rho_ext 
                 * static_cast<EulerEquationsWeakFormImplicitMultiComponent*>(wf)->v1_ext;
        w_B[2] = static_cast<EulerEquationsWeakFormImplicitMultiComponent*>(wf)->rho_ext 
                 * static_cast<EulerEquationsWeakFormImplicitMultiComponent*>(wf)->v2_ext;
        w_B[3] = static_cast<EulerEquationsWeakFormImplicitMultiComponent*>(wf)->energy_ext;
    
        double flux[4];
        num_flux->numerical_flux_inlet(flux, w_L, w_B, e->nx[i], e->ny[i]);

        result_0 -= wt[i] * v->val[i] * flux[0];
        result_1 -= wt[i] * v->val[i] * flux[1];
        result_2 -= wt[i] * v->val[i] * flux[2];
        result_3 -= wt[i] * v->val[i] * flux[3];
      }
      result.push_back(result_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    NumericalFlux* num_flux;
  };

  class EulerEquationsLinearFormOutlet : public WeakForm::MultiComponentVectorFormSurf
  {
  public:
    EulerEquationsLinearFormOutlet(Hermes::vector<unsigned int> coordinates, 
                                   std::string marker, NumericalFlux* num_flux) : 
    WeakForm::MultiComponentVectorFormSurf(coordinates, marker), num_flux(num_flux) {}

    void value(int n, double *wt, Func<double> *u_ext[], Func<double> *v, Geom<double> *e, 
               ExtData<double> *ext, Hermes::vector<double>& result) const {
      double result_0 = 0;
      double result_1 = 0;
      double result_2 = 0;
      double result_3 = 0;
      double w_L[4];
      for (int i = 0; i < n; i++) {
        w_L[0] = u_ext[0]->val[i];
        w_L[1] = u_ext[1]->val[i];
        w_L[2] = u_ext[2]->val[i];
        w_L[3] = u_ext[3]->val[i];

        double flux[4];
        num_flux->numerical_flux_outlet(flux, w_L, 
          static_cast<EulerEquationsWeakFormImplicitMultiComponent*>(wf)->pressure_ext, e->nx[i], e->ny[i]);
        result_0 -= wt[i] * v->val[i] * flux[0];
        result_1 -= wt[i] * v->val[i] * flux[1];
        result_2 -= wt[i] * v->val[i] * flux[2];
        result_3 -= wt[i] * v->val[i] * flux[3];
      }

      result.push_back(result_0 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_1 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_2 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
      result.push_back(result_3 * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau());
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, ExtData<Ord> *ext) const {
      return v->val[0];
    }

    // Members.
    NumericalFlux* num_flux;
  };

  // Members.
  double rho_ext;
  double v1_ext;
  double v2_ext;
  double pressure_ext;
  double energy_ext;
  double tau;
};
// The parameter variant in the constructor has the following meaning:
// 1 - Dirichlet condition (concentration production) on the inlet.
// 2 - Dirichlet condition (concentration production) on the bottom.
// 3 - Dirichlet condition (concentration production) on the top.
class EulerEquationsWeakFormImplicitCoupled : public EulerEquationsWeakFormImplicit
{
public:
  // Constructor.
  EulerEquationsWeakFormImplicitCoupled(int variant, NumericalFlux* num_flux, double kappa, 
                                        double rho_ext, double v1_ext, double v2_ext, 
                                        double pressure_ext, std::string solid_wall_bottom_marker, 
                                        std::string solid_wall_top_marker, std::string inlet_marker, 
                                        std::string outlet_marker, Solution* prev_density, 
                                        Solution* prev_density_vel_x, Solution* prev_density_vel_y, 
                                        Solution* prev_energy, Solution* prev_concentration, 
                                        bool preconditioning, double epsilon) 
       : EulerEquationsWeakFormImplicit(num_flux, kappa, rho_ext, v1_ext, v2_ext, pressure_ext, 
                                        solid_wall_bottom_marker, solid_wall_top_marker, inlet_marker, 
                                        outlet_marker, prev_density, prev_density_vel_x, 
                                        prev_density_vel_y, prev_energy, preconditioning, 5) {
    
    if(preconditioning)
      add_matrix_form(new EulerEquationsPreconditioning(4));
    
    EulerEquationsLinearFormTime* linear_form_time = new EulerEquationsLinearFormTime(4);
    linear_form_time->ext.push_back(prev_density);
    linear_form_time->ext.push_back(prev_density_vel_x);
    linear_form_time->ext.push_back(prev_density_vel_y);
    linear_form_time->ext.push_back(prev_energy);
    linear_form_time->ext.push_back(prev_concentration);
    add_vector_form(linear_form_time);
  
    add_vector_form(new VectorFormConcentrationDiffusion(4, epsilon));
    add_vector_form(new VectorFormConcentrationAdvection(4));

    if(variant != 1)
      add_vector_form_surf(new VectorFormConcentrationNatural(4, inlet_marker));
    if(variant != 2)
      add_vector_form_surf(new VectorFormConcentrationNatural(4, solid_wall_bottom_marker));
    if(variant != 3)
      add_vector_form_surf(new VectorFormConcentrationNatural(4, solid_wall_top_marker));

    add_vector_form_surf(new VectorFormConcentrationNatural(4, outlet_marker));

    add_vector_form_surf(new VectorFormConcentrationInterface (4));
  };

  // Destructor.
  ~EulerEquationsWeakFormImplicitCoupled() {};
protected:
  class VectorFormConcentrationDiffusion : public WeakForm::VectorFormVol
  {
  public:
    VectorFormConcentrationDiffusion(int i, double epsilon) 
          : WeakForm::VectorFormVol(i), epsilon(epsilon) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      Func<Real>* concentration_prev = u_ext[4];
      return - epsilon * int_grad_u_grad_v<Real, Scalar>(n, wt, concentration_prev, v) 
             * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }

    // Member.
    double epsilon;
  };

  class VectorFormConcentrationAdvection : public WeakForm::VectorFormVol
  {
  public:
    VectorFormConcentrationAdvection(int i) : WeakForm::VectorFormVol(i) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      Func<Real>* density_prev = u_ext[0];
      Func<Real>* density_vel_x_prev = u_ext[1];
      Func<Real>* density_vel_y_prev = u_ext[2];
      Func<Real>* concentration_prev = u_ext[4];

      Scalar result = 0;
      for (int i = 0; i < n; i++)
        result += wt[i] * concentration_prev->val[i] * ((density_vel_x_prev->val[i] 
                  * v->dx[i]) + (density_vel_y_prev->val[i] * v->dy[i]))
                  / density_prev->val[i];
      return result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }
  };

  class VectorFormConcentrationNatural : public WeakForm::VectorFormSurf
  {
  public:
    VectorFormConcentrationNatural(int i, std::string marker) 
          : WeakForm::VectorFormSurf(i, marker) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      Func<Real>* density_prev = u_ext[0];
      Func<Real>* density_vel_x_prev = u_ext[1];
      Func<Real>* density_vel_y_prev = u_ext[2];
      Func<Real>* concentration_prev = u_ext[4];

      Scalar result = 0;
      for (int i = 0; i < n; i++)
        result += wt[i] * v->val[i] * concentration_prev->val[i] 
                  * (density_vel_x_prev->val[i] * e->nx[i] + density_vel_y_prev->val[i] * e->ny[i])
                  / density_prev->val[i];
        // (OR: for inlet/outlet) result += wt[i] * v->val[i] * concentration_prev->val[i] 
        //      * (V1_EXT * e->nx[i] + V2_EXT * e->ny[i]);
      return - result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, 
                 Geom<double> *e, ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return vector_form<Ord, Ord>(n, wt, u_ext, v, e, ext);
    }
  };

  class VectorFormConcentrationInterface : public WeakForm::VectorFormSurf
  {
  public:
    VectorFormConcentrationInterface(int i) : WeakForm::VectorFormSurf(i, H2D_DG_INNER_EDGE) {}

    template<typename Real, typename Scalar>
    Scalar vector_form(int n, double *wt, Func<Scalar> *u_ext[], Func<Real> *v, 
                       Geom<Real> *e, ExtData<Scalar> *ext) const {
      Func<Real>* density_prev = u_ext[0];
      Func<Real>* density_vel_x_prev = u_ext[1];
      Func<Real>* density_vel_y_prev = u_ext[2];
      Func<Real>* concentration_prev = u_ext[4];

      Scalar result = 0;
      for (int i = 0; i < n; i++)
        result += wt[i] * v->val[i] *
                  (
                    (
                      density_vel_x_prev->get_val_central(i) * concentration_prev->get_val_central(i) 
                      / density_prev->get_val_central(i)
                      -
                      density_vel_x_prev->get_val_neighbor(i) * concentration_prev->get_val_neighbor(i) 
                      / density_prev->get_val_neighbor(i)
                    ) * e->nx[i]
                    + 
                    (
                      density_vel_y_prev->get_val_central(i) * concentration_prev->get_val_central(i) 
                      / density_prev->get_val_central(i)
                      -
                      density_vel_y_prev->get_val_neighbor(i) * concentration_prev->get_val_neighbor(i) 
                      / density_prev->get_val_neighbor(i)
                    ) * e->ny[i]
                  );
      return - result * static_cast<EulerEquationsWeakFormImplicit*>(wf)->get_tau();
    }

    scalar value(int n, double *wt, Func<scalar> *u_ext[], Func<double> *v, Geom<double> *e, 
                 ExtData<scalar> *ext) const {
      return vector_form<double, scalar>(n, wt, u_ext, v, e, ext);
    }

    Ord ord(int n, double *wt, Func<Ord> *u_ext[], Func<Ord> *v, Geom<Ord> *e, 
            ExtData<Ord> *ext) const {
      return Ord(20);
    }
  };
};
