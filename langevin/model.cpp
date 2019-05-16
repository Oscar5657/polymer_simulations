//*******************************************************************************
//*
//* Langevin Dynamics
//*	model.cpp
//*	Implementation of model.h
//*
//* Author: Guillaume Le Treut
//*	CEA - 2014
//*
//*******************************************************************************
#include "model.h"

using namespace std;

//****************************************************************************
//* LangevinSimulation
//****************************************************************************
//LangevinSimulation::LangevinSimulation(size_t itermax, double temp, double dt) :
//  m_itermax(itermax), m_temp(temp), m_dt(dt) {
//  m_s2dt = sqrt(2.*m_dt);
//}
//
//LangevinSimulation::~LangevinSimulation(){
//}
//
//void
//LangevinSimulation::update() {
//  /*
//   * Update all objects
//   */
//  for (vector<MDWorld>::iterator it=m_objs.begin(); it != m_objs.end(); ++it){
//    it->update();
//  }
//  return;
//}

//****************************************************************************
//* MDWorld
//****************************************************************************
MDWorld::MDWorld(size_t npart, double lx, double ly, double lz, double sig_hard_core, double gamma, double temp, double mass) :
  m_npart(npart), m_lx(lx), m_ly(ly), m_lz(lz), m_sig_hard_core(sig_hard_core), m_gamma(gamma), m_temp(temp), m_mass(mass) {
  /* check */
  if (m_npart == 0){
    throw invalid_argument("Number of particles must be larger than zero!");
  }

  /* initializations */
  m_energy_pot=0.;
  m_energy_kin=0.;

  m_x = gsl_matrix_calloc(m_npart, 3);
  gsl_matrix_set_all(m_x, 0.0);
  m_v = gsl_matrix_calloc(m_npart, 3);
  gsl_matrix_set_all(m_v, 0.0);
  m_forces = gsl_matrix_calloc(m_npart, 3);
  gsl_matrix_set_all(m_forces, 0.0);
  m_forces_tp = gsl_matrix_calloc(m_npart, 3);
  gsl_matrix_set_all(m_forces_tp, 0.0);

  m_ffields.clear();
}

MDWorld::~MDWorld() {
  gsl_matrix_free(m_x);
  gsl_matrix_free(m_v);
  gsl_matrix_free(m_forces);
  gsl_matrix_free(m_forces_tp);

  for (vector<ForceField*>::iterator it=m_ffields.begin(); it!=m_ffields.end(); ++it){
    if (*it != nullptr){
      delete (*it);
    }
  }
}

void MDWorld::init_positions(){
  /*
   * Initialize positions
   */

  size_t counter;
  size_t nx, ny, nz;
  double delta;
  double x,y,z;

  delta = m_sig_hard_core*pow(2.,1./6);
  nx = m_lx/delta;
  ny = m_ly/delta;
  nz = m_lz/delta;

  counter = 0;
  for (size_t ix = 1; ix < nx; ++ix){
    x = -0.5*m_lx + ix*delta;
    for (size_t iy = 1; iy < ny; ++iy){
      y = -0.5*m_ly + iy*delta;
      for (size_t iz = 1; iz < nz; ++iz){
        z = -0.5*m_lz + iz*delta;
        gsl_matrix_set(m_x, counter, 0, x);
        gsl_matrix_set(m_x, counter, 1, y);
        gsl_matrix_set(m_x, counter, 2, z);
        counter += 1;

        if (counter == m_npart)
          break;
      }
      if (counter == m_npart)
        break;
    }
    if (counter == m_npart)
      break;
  }

  if (counter < m_npart) {
    throw runtime_error("Box is too small to position all particles on a lattice!");
  }

  return;
}

void MDWorld::init_velocities(gsl_rng *rng){
  /*
   * Initialize velocities
   */
  // declarations
  double vxm, vym, vzm, vsqm, vvar;
  double vx, vy, vz;
  gsl_vector *vm(0);
  double fs;

  // initializations
  vxm = 0.;
  vym = 0.;
  vzm = 0.;
  vsqm = 0.;
  vm = gsl_vector_calloc(3);

  // random velocities
  for (size_t n=0; n<m_npart; ++n){
    vx = gsl_rng_uniform(rng);
    vy = gsl_rng_uniform(rng);
    vz = gsl_rng_uniform(rng);
    gsl_matrix_set(m_v, n, 0, vx);
    gsl_matrix_set(m_v, n, 1, vy);
    gsl_matrix_set(m_v, n, 2, vz);

    // update average velocity
    vxm += vx;
    vym += vy;
    vzm += vz;

    // update average square velocity
    vsqm += vx*vx + vy*vy + vz*vz;
  }

  // compute mean velocity
  vxm /= m_npart;
  vym /= m_npart;
  vzm /= m_npart;
  gsl_vector_set(vm, 0, vxm);
  gsl_vector_set(vm, 1, vym);
  gsl_vector_set(vm, 2, vzm);

  // compute variance
  vsqm /= m_npart;
  vvar = vsqm - (vxm*vxm + vym*vym + vzm*vzm);  // var = <v^2> - <v>^2

  // shift and rescale velocities
  if (m_npart == 1){
    fs = sqrt(3.*m_temp / vsqm);
      gsl_vector_view v = gsl_matrix_row(m_v, 0);
      linalg_dscal(fs, &v.vector);
  }
  else {
    fs = sqrt(3.*m_temp / vvar);
    for (size_t n=0; n<m_npart; ++n){
      gsl_vector_view v = gsl_matrix_row(m_v, n);
      // shift
      linalg_daxpy(-1., vm, &v.vector);

      // rescale
      linalg_dscal(fs, &v.vector);
    }
  }

  // exit
  gsl_vector_free(vm);
}

void MDWorld::update_energy_forces(){
  /*
   * compute all forces
   */
  m_energy_pot = 0;
  gsl_matrix_set_all(m_forces, 0.);

  /* iterate over force fields */
  for (vector<ForceField*>::iterator it=m_ffields.begin(); it!=m_ffields.end(); ++it){
      (*it)->energy_force(m_x, &m_energy_pot, m_forces);
  }

  return;
}

void MDWorld::update_energy_kinetics(){
  /*
   * compute all forces
   */

  m_energy_kin = 0.5*m_mass*linalg_ddot(m_v, m_v);

  return;
}

void
MDWorld::dump_thermo(ostream &mystream){
  /*
   * Dump thermodynamic quantities.
   */

  mystream << left << dec;
  mystream << setw(10) << setprecision(0) << fixed << noshowpos << m_npart;
  mystream << setw(18) << setprecision(8) << scientific << showpos << m_energy_pot;
  mystream << setw(18) << setprecision(8) << scientific << showpos << m_energy_kin;
  return;
}

void
MDWorld::dump_pos(ostream &mystream, bool index, bool positions, bool velocities, bool forces){
  /*
   * Dump configuration
   */

	if (m_npart == 0)
		throw invalid_argument("MDWorld is empty!");

  mystream << left << dec << fixed;

	for (size_t i=0; i<m_npart;i++){
    if (index) {
      mystream << setw(10) << setprecision(0) << noshowpos << i;
    }
    if (positions) {
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_x,i,0);
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_x,i,1);
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_x,i,2);
    }
    if (velocities) {
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_v,i,0);
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_v,i,1);
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_v,i,2);
    }
    if (forces) {
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_forces,i,0);
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_forces,i,1);
      mystream << setw(18) << setprecision(8) << showpos << gsl_matrix_get(m_forces,i,2);
    }
		mystream << endl;
	}
  return;
}

void
MDWorld::dump_dat(std::string fileout) {
  /*
   * Dump configuration in .dat format.
   */
  ofstream fout;
  fout.open(fileout.c_str());

  dump_pos(fout, true, true, true, true);

  fout.close();
  return;
}

void
MDWorld::dump_xyz(std::string fileout, size_t iter) {
  /*
   * Dump configuration in .xyz format.
   */
  ofstream fout;
  fout.open(fileout.c_str());

  fout << left << dec << fixed << setw(10) << setprecision(0) << noshowpos << m_npart << endl;
  fout << "Atoms. Timestep:" << setw(10) << iter << endl;
  dump_pos(fout, true, true, false, false);

  fout.close();
  return;
}

void
MDWorld::load_dat(std::string filein) {
  /*
   * Load configuration in .dat format.
   */
  /* declarations */
  ifstream fin;
  stringstream convert;
  string line;

  size_t n;
  double rx, ry, rz, vx, vy, vz;

  /* initializations */
  fin.open(filein.c_str());

  /* reading file */
  while (getline(fin, line)) {
    convert.clear();
    convert.str(line);
    if (!(
          (convert >> n) &&
          (convert >> rx) &&
          (convert >> ry) &&
          (convert >> rz) &&
          (convert >> vx) &&
          (convert >> vy) &&
          (convert >> vz)
         )
       ) {
      throw runtime_error("Problem in configuration import");
    }

    /* checks */
    if ( !(n < m_npart) ){
      cout << "n = " << n << endl;
      throw runtime_error("Problem in configuration import: n too large!");
    }

    // set position
    gsl_matrix_set(m_x, n, 0, rx);
    gsl_matrix_set(m_x, n, 1, ry);
    gsl_matrix_set(m_x, n, 2, rz);

    // set velocity
    gsl_matrix_set(m_v, n, 0, vx);
    gsl_matrix_set(m_v, n, 1, vy);
    gsl_matrix_set(m_v, n, 2, vz);

    // verbose
    cout << "particle " << n << " loaded." << endl;
  }

  fin.close();
  return;
}

void
MDWorld::load_xyz(std::string filein) {
  /*
   * Load configuration in .xyz format.
   */
  /* declarations */
  ifstream fin;
  stringstream convert;
  string line;

  size_t n,skiprows;
  double rx, ry, rz;

  /* initializations */
  skiprows=2;
  fin.open(filein.c_str());

  /* reading file */
  for (size_t i=0; i<skiprows; ++i){
    getline(fin,line);
  }

  while (getline(fin, line)) {
    convert.clear();
    convert.str(line);
    if (!(
          (convert >> n) &&
          (convert >> rx) &&
          (convert >> ry) &&
          (convert >> rz)
         )
       ) {
      throw runtime_error("Problem in configuration import");
    }

    /* checks */
    if ( !(n < m_npart) ){
      cout << "n = " << n << endl;
      throw runtime_error("Problem in configuration import: n too large!");
    }

    // set position
    gsl_matrix_set(m_x, n, 0, rx);
    gsl_matrix_set(m_x, n, 1, ry);
    gsl_matrix_set(m_x, n, 2, rz);

    // verbose
    cout << "particle " << n << " loaded." << endl;
  }

  fin.close();
  return;
}
