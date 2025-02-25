// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \author R+Preghenella - January 2020

#include "Generators/GeneratorPythia8.h"
#include "Generators/GeneratorPythia8Param.h"
#include "CommonUtils/ConfigurationMacroHelper.h"
#include <fairlogger/Logger.h>
#include "TParticle.h"
#include "TF1.h"
#include "TRandom.h"
#include "SimulationDataFormat/MCEventHeader.h"
#include "SimulationDataFormat/MCGenStatus.h"
#include "SimulationDataFormat/ParticleStatus.h"
#include "Pythia8/HIUserHooks.h"
#include "TSystem.h"
#include "ZDCBase/FragmentParam.h"

#include <iostream>

namespace o2
{
namespace eventgen
{

/*****************************************************************/
/*****************************************************************/

GeneratorPythia8::GeneratorPythia8() : Generator("ALICEo2", "ALICEo2 Pythia8 Generator")
{
  /** default constructor **/

  mInterface = reinterpret_cast<void*>(&mPythia);
  mInterfaceName = "pythia8";

  auto& param = GeneratorPythia8Param::Instance();
  LOG(info) << "Instance \'Pythia8\' generator with following parameters";
  LOG(info) << param;

  setConfig(param.config);
  setHooksFileName(param.hooksFileName);
  setHooksFuncName(param.hooksFuncName);
}

/*****************************************************************/

GeneratorPythia8::GeneratorPythia8(const Char_t* name, const Char_t* title) : Generator(name, title)
{
  /** constructor **/

  mInterface = reinterpret_cast<void*>(&mPythia);
  mInterfaceName = "pythia8";
}

/*****************************************************************/

Bool_t GeneratorPythia8::Init()
{
  /** init **/

  /** init base class **/
  Generator::Init();

  /** read configuration **/
  if (!mConfig.empty()) {
    std::stringstream ss(mConfig);
    std::string config;
    while (getline(ss, config, ' ')) {
      config = gSystem->ExpandPathName(config.c_str());
      LOG(info) << "Reading configuration from file: " << config;
      if (!mPythia.readFile(config, true)) {
        LOG(fatal) << "Failed to init \'Pythia8\': problems with configuration file "
                   << config;
        return false;
      }
    }
  }

  /** user hooks via configuration macro **/
  if (!mHooksFileName.empty()) {
    LOG(info) << "Applying \'Pythia8\' user hooks: " << mHooksFileName << " -> " << mHooksFuncName;
    auto hooks = o2::conf::GetFromMacro<Pythia8::UserHooks*>(mHooksFileName, mHooksFuncName, "Pythia8::UserHooks*", "pythia8_user_hooks");
    if (!hooks) {
      LOG(fatal) << "Failed to init \'Pythia8\': problem with user hooks configuration ";
      return false;
    }
    setUserHooks(hooks);
  }

#if PYTHIA_VERSION_INTEGER < 8300
  /** [NOTE] The issue with large particle production vertex when running
      Pythia8 heavy-ion model (Angantyr) is solved in Pythia 8.3 series.
      For discussions about this issue, please refer to this JIRA ticket
      https://alice.its.cern.ch/jira/browse/O2-1382.
      The code remains within preprocessor directives, both for reference
      and in case future use demands to roll back to Pythia 8.2 series. **/

  /** inhibit hadron decays **/
  mPythia.readString("HadronLevel:Decay off");
#endif

  /** initialise **/
  if (!mPythia.init()) {
    LOG(fatal) << "Failed to init \'Pythia8\': init returned with error";
    return false;
  }

  /** success **/
  return true;
}

/*****************************************************************/

Bool_t
  GeneratorPythia8::generateEvent()
{
  /** generate event **/

  /** generate event **/
  if (!mPythia.next()) {
    return false;
  }

#if PYTHIA_VERSION_INTEGER < 8300
  /** [NOTE] The issue with large particle production vertex when running
      Pythia8 heavy-ion model (Angantyr) is solved in Pythia 8.3 series.
      For discussions about this issue, please refer to this JIRA ticket
      https://alice.its.cern.ch/jira/browse/O2-1382.
      The code remains within preprocessor directives, both for reference
      and in case future use demands to roll back to Pythia 8.2 series. **/

  /** As we have inhibited all hadron decays before init,
      the event generation stops after hadronisation.
      We then pick all particles from here and force their
      production vertex to be (0,0,0,0).
      Afterwards we process the decays. **/

  /** force production vertices to (0,0,0,0) **/
  auto nParticles = mPythia.event.size();
  for (int iparticle = 0; iparticle < nParticles; iparticle++) {
    auto& aParticle = mPythia.event[iparticle];
    aParticle.xProd(0.);
    aParticle.yProd(0.);
    aParticle.zProd(0.);
    aParticle.tProd(0.);
  }

  /** proceed with decays **/
  if (!mPythia.moreDecays())
    return false;
#endif

  /** success **/
  return true;
}

/*****************************************************************/

Bool_t
  GeneratorPythia8::importParticles(Pythia8::Event& event)
{
  /** import particles **/

  /* loop over particles */
  //  auto weight = mPythia.info.weight(); // TBD: use weights
  auto nParticles = event.size();
  for (Int_t iparticle = 1; iparticle < nParticles; iparticle++) { // first particle is system
    auto particle = event[iparticle];
    auto pdg = particle.id();
    auto st = o2::mcgenstatus::MCGenStatusEncoding(particle.statusHepMC(), particle.status()).fullEncoding;
    auto px = particle.px();
    auto py = particle.py();
    auto pz = particle.pz();
    auto et = particle.e();
    auto vx = particle.xProd();
    auto vy = particle.yProd();
    auto vz = particle.zProd();
    auto vt = particle.tProd();
    auto m1 = particle.mother1() - 1;
    auto m2 = particle.mother2() - 1;
    auto d1 = particle.daughter1() - 1;
    auto d2 = particle.daughter2() - 1;
    mParticles.push_back(TParticle(pdg, st, m1, m2, d1, d2, px, py, pz, et, vx, vy, vz, vt));
    mParticles.back().SetBit(ParticleStatus::kToBeDone, particle.statusHepMC() == 1);
  }

  /** success **/
  return kTRUE;
}

/*****************************************************************/

void GeneratorPythia8::updateHeader(o2::dataformats::MCEventHeader* eventHeader)
{
  /** update header **/

  eventHeader->putInfo<std::string>("generator", "pythia8");
  eventHeader->putInfo<int>("version", PYTHIA_VERSION_INTEGER);
  eventHeader->putInfo<std::string>("processName", mPythia.info.name());
  eventHeader->putInfo<int>("processCode", mPythia.info.code());

#if PYTHIA_VERSION_INTEGER < 8300
  auto hiinfo = mPythia.info.hiinfo;
#else
  auto hiinfo = mPythia.info.hiInfo;
#endif

  if (hiinfo) {
    /** set impact parameter **/
    eventHeader->SetB(hiinfo->b());
    eventHeader->putInfo<double>("Bimpact", hiinfo->b());
    auto bImp = hiinfo->b();
    /** set Ncoll, Npart and Nremn **/
    int nColl, nPart;
    int nPartProtonProj, nPartNeutronProj, nPartProtonTarg, nPartNeutronTarg;
    int nRemnProtonProj, nRemnNeutronProj, nRemnProtonTarg, nRemnNeutronTarg;
    int nFreeNeutronProj, nFreeProtonProj, nFreeNeutronTarg, nFreeProtonTarg;
    getNcoll(nColl);
    getNpart(nPart);
    getNpart(nPartProtonProj, nPartNeutronProj, nPartProtonTarg, nPartNeutronTarg);
    getNremn(nRemnProtonProj, nRemnNeutronProj, nRemnProtonTarg, nRemnNeutronTarg);
    getNfreeSpec(nFreeNeutronProj, nFreeProtonProj, nFreeNeutronTarg, nFreeProtonTarg);
    eventHeader->putInfo<int>("Ncoll", nColl);
    eventHeader->putInfo<int>("Npart", nPart);
    eventHeader->putInfo<int>("Npart_proj_p", nPartProtonProj);
    eventHeader->putInfo<int>("Npart_proj_n", nPartNeutronProj);
    eventHeader->putInfo<int>("Npart_targ_p", nPartProtonTarg);
    eventHeader->putInfo<int>("Npart_targ_n", nPartNeutronTarg);
    eventHeader->putInfo<int>("Nremn_proj_p", nRemnProtonProj);
    eventHeader->putInfo<int>("Nremn_proj_n", nRemnNeutronProj);
    eventHeader->putInfo<int>("Nremn_targ_p", nRemnProtonTarg);
    eventHeader->putInfo<int>("Nremn_targ_n", nRemnNeutronTarg);
    eventHeader->putInfo<int>("Nfree_proj_n", nFreeNeutronProj);
    eventHeader->putInfo<int>("Nfree_proj_p", nFreeProtonProj);
    eventHeader->putInfo<int>("Nfree_targ_n", nFreeNeutronTarg);
    eventHeader->putInfo<int>("Nfree_targ_p", nFreeProtonTarg);
  }
}

/*****************************************************************/

void GeneratorPythia8::selectFromAncestor(int ancestor, Pythia8::Event& inputEvent, Pythia8::Event& outputEvent)
{

  /** select from ancestor
      fills the output event with all particles related to
      an ancestor of the input event **/

  // recursive selection via lambda function
  std::set<int> selected;
  std::function<void(int)> select;
  select = [&](int i) {
    selected.insert(i);
    auto dl = inputEvent[i].daughterList();
    for (auto j : dl) {
      select(j);
    }
  };
  select(ancestor);

  // map selected particle index to output index
  std::map<int, int> indexMap;
  int index = outputEvent.size();
  for (auto i : selected) {
    indexMap[i] = index++;
  }

  // adjust mother/daughter indices and append to output event
  for (auto i : selected) {
    auto p = mPythia.event[i];
    auto m1 = indexMap[p.mother1()];
    auto m2 = indexMap[p.mother2()];
    auto d1 = indexMap[p.daughter1()];
    auto d2 = indexMap[p.daughter2()];
    p.mothers(m1, m2);
    p.daughters(d1, d2);

    outputEvent.append(p);
  }
}

/*****************************************************************/

void GeneratorPythia8::getNcoll(const Pythia8::Info& info, int& nColl)
{

  /** compute number of collisions from sub-collision information **/

#if PYTHIA_VERSION_INTEGER < 8300
  auto hiinfo = info.hiinfo;
#else
  auto hiinfo = info.hiInfo;
#endif

  nColl = 0;

  if (!hiinfo) {
    return;
  }

  // loop over sub-collisions
  auto scptr = hiinfo->subCollisionsPtr();
  for (auto sc : *scptr) {

    // wounded nucleon flag in projectile/target
    auto pW = sc.proj->status() == Pythia8::Nucleon::ABS; // according to C.Bierlich this should be == Nucleon::ABS
    auto tW = sc.targ->status() == Pythia8::Nucleon::ABS;

    // increase number of collisions if both are wounded
    if (pW && tW) {
      nColl++;
    }
  }
}

/*****************************************************************/

void GeneratorPythia8::getNpart(const Pythia8::Info& info, int& nPart)
{

  /** compute number of participants as the sum of all participants nucleons **/

  int nProtonProj, nNeutronProj, nProtonTarg, nNeutronTarg;
  getNpart(info, nProtonProj, nNeutronProj, nProtonTarg, nNeutronTarg);
  nPart = nProtonProj + nNeutronProj + nProtonTarg + nNeutronTarg;
}

/*****************************************************************/

void GeneratorPythia8::getNpart(const Pythia8::Info& info, int& nProtonProj, int& nNeutronProj, int& nProtonTarg, int& nNeutronTarg)
{

  /** compute number of participants from sub-collision information **/

#if PYTHIA_VERSION_INTEGER < 8300
  auto hiinfo = info.hiinfo;
#else
  auto hiinfo = info.hiInfo;
#endif

  nProtonProj = nNeutronProj = nProtonTarg = nNeutronTarg = 0;
  if (!hiinfo) {
    return;
  }

  // keep track of wounded nucleons
  std::vector<Pythia8::Nucleon*> projW;
  std::vector<Pythia8::Nucleon*> targW;

  // loop over sub-collisions
  auto scptr = hiinfo->subCollisionsPtr();
  for (auto sc : *scptr) {

    // wounded nucleon flag in projectile/target
    auto pW = sc.proj->status() == Pythia8::Nucleon::ABS || sc.proj->status() == Pythia8::Nucleon::DIFF; // according to C.Bierlich this should be == Nucleon::ABS || Nucleon::DIFF
    auto tW = sc.targ->status() == Pythia8::Nucleon::ABS || sc.targ->status() == Pythia8::Nucleon::DIFF;

    // increase number of wounded projectile nucleons if not yet in the wounded vector
    if (pW && std::find(projW.begin(), projW.end(), sc.proj) == projW.end()) {
      projW.push_back(sc.proj);
      if (sc.proj->id() == 2212) {
        nProtonProj++;
      } else if (sc.proj->id() == 2112) {
        nNeutronProj++;
      }
    }

    // increase number of wounded target nucleons if not yet in the wounded vector
    if (tW && std::find(targW.begin(), targW.end(), sc.targ) == targW.end()) {
      targW.push_back(sc.targ);
      if (sc.targ->id() == 2212) {
        nProtonTarg++;
      } else if (sc.targ->id() == 2112) {
        nNeutronTarg++;
      }
    }
  }
}

/*****************************************************************/

void GeneratorPythia8::getNremn(const Pythia8::Event& event, int& nProtonProj, int& nNeutronProj, int& nProtonTarg, int& nNeutronTarg)
{

  /** compute number of spectators from the nuclear remnant of the beams **/

  // reset
  nProtonProj = nNeutronProj = nProtonTarg = nNeutronTarg = 0;
  auto nNucRem = 0;

  // particle loop
  auto nparticles = event.size();
  for (int ipa = 0; ipa < nparticles; ++ipa) {
    const auto particle = event[ipa];
    auto pdg = particle.id();

    // nuclear remnants have pdg code = ±10LZZZAAA9
    if (pdg < 1000000000) {
      continue; // must be nucleus
    }
    if (pdg % 10 != 9) {
      continue; // first digit must be 9
    }
    nNucRem++;

    // extract A, Z and L from pdg code
    pdg /= 10;
    auto A = pdg % 1000;
    pdg /= 1000;
    auto Z = pdg % 1000;
    pdg /= 1000;
    auto L = pdg % 10;

    if (particle.pz() > 0.) {
      nProtonProj = Z;
      nNeutronProj = A - Z;
    }
    if (particle.pz() < 0.) {
      nProtonTarg = Z;
      nNeutronTarg = A - Z;
    }

  } // end of particle loop

  if (nNucRem > 2) {
    LOG(warning) << " GeneratorPythia8: found more than two nuclear remnants (weird)";
  }
}
/*****************************************************************/

/*****************************************************************/

void GeneratorPythia8::getNfreeSpec(const Pythia8::Info& info, int& nFreenProj, int& nFreepProj, int& nFreenTarg, int& nFreepTarg)
{
  /** compute number of free spectator nucleons for ZDC response **/

#if PYTHIA_VERSION_INTEGER < 8300
  auto hiinfo = info.hiinfo;
#else
  auto hiinfo = info.hiInfo;
#endif

  if (!hiinfo) {
    return;
  }

  double b = hiinfo->b();

  static o2::zdc::FragmentParam frag; // data-driven model to get free spectators given impact parameter

  TF1 const& fneutrons = frag.getfNeutrons();
  TF1 const& fsigman = frag.getsigmaNeutrons();
  TF1 const& fprotons = frag.getfProtons();
  TF1 const& fsigmap = frag.getsigmaProtons();

  // Calculating no. of free spectators from parametrization
  int nneu[2] = {0, 0};
  for (int i = 0; i < 2; i++) {
    float nave = fneutrons.Eval(b);
    float sigman = fsigman.Eval(b);
    float nfree = gRandom->Gaus(nave, 0.68 * sigman * nave);
    nneu[i] = (int)nfree;
    if (nave < 0 || nneu[i] < 0) {
      nneu[i] = 0;
    }
    if (nneu[i] > 126) {
      nneu[i] = 126;
    }
  }
  //
  int npro[2] = {0, 0};
  for (int i = 0; i < 2; i++) {
    float pave = fprotons.Eval(b);
    float sigmap = fsigman.Eval(b);
    float pfree = gRandom->Gaus(pave, 0.68 * sigmap * pave) / 0.7;
    npro[i] = (int)pfree;
    if (pave < 0 || npro[i] < 0) {
      npro[i] = 0;
    }
    if (npro[i] > 82) {
      npro[i] = 82;
    }
  }

  nFreenProj = nneu[0];
  nFreenTarg = nneu[1];
  nFreepProj = npro[0];
  nFreepTarg = npro[1];
  /*****************************************************************/
}

} /* namespace eventgen */
} /* namespace o2 */
