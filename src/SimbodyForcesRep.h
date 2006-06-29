#ifndef SimTK_SIMBODY_FORCES_REP_H_
#define SimTK_SIMBODY_FORCES_REP_H_

/* Copyright (c) 2006 Stanford University and Michael Sherman.
 * Contributors:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** @file
 * Define the private implementations of some basic types of force
 * subsystems of use to Simbody users.
 */

#include "SimTKcommon.h"
#include "simbody/internal/common.h"
#include "simbody/internal/State.h"
#include "simbody/internal/MultibodySystem.h"
#include "simbody/internal/MatterSubsystem.h"
#include "simbody/internal/SimbodyForces.h"

#include "MultibodySystemRep.h"
#include "ForceSubsystemRep.h"


namespace SimTK {

// 
// Define a linear spring between two stations s1 and s2 of a matter subsystem
// (a station is a point fixed on a particular body). A spring has a stiffness k, 
// and a natural length x0 at which it generates no force. Define the separation
// vector v=s2-s1, with x=|v| the spring's current length.
//
// We will request parameters in the State for k and x0 but require fixed stations.
// Defaults for k and x0 must be provided on construction.
//
// Then the potential energy stored in the spring is 
//    pe = k(x-x0)^2/2
// Forces are generated on both points, as the negative gradient of the
// potential energy at that point: 
//    f1 = d pe/d s1 =  k(x-x0)v/x
//    f2 = d pe/d s2 = -k(x-x0)v/x.
// Note that force is undefined when x=0; we'll return NaN vectors in that case.

class TwoPointSpringSubsystemRep : public ForceSubsystemRep {

    // state entries
    struct Parameters {
        Parameters(const Real& k, const Real& x0) 
            : stiffness(k), naturalLength(x0), gravity(0), damping(0) { }
        Real stiffness, naturalLength;
        Vec3 gravity;
        Real damping;
    };

    struct ConfigurationCache {
        Vec3 station1_G, station2_G; // body station vectors re-expressed in G
        Vec3 v_G;
        Real x;       // length of above vector
        Real fscalar; // k(x-x0)
        Real pe;
    };
    struct DynamicsCache {
        Vec3 f1_G;    // f2 is the negative of this
    };

    // topological variables
    int  body1, body2;
    Vec3 station1, station2;
    Parameters defaultParameters;

    // These must be filled in during realizeConstruction and treated
    // as const thereafter. These are garbage unless built=true.
    mutable int parameterVarsIndex;
    mutable int configurationCacheIndex;
    mutable int dynamicsCacheIndex;
    mutable bool built;

    const Parameters& getParameters(const State& s) const {
        return Value<Parameters>::downcast(
            getDiscreteVariable(s,parameterVarsIndex)).get();
    }
    Parameters& updParameters(State& s) const {
        return Value<Parameters>::downcast(
            updDiscreteVariable(s,parameterVarsIndex)).upd();
    }
    const ConfigurationCache& getConfigurationCache(const State& s) const {
        return Value<ConfigurationCache>::downcast(
            getCacheEntry(s,configurationCacheIndex)).get();
    }
    ConfigurationCache& updConfigurationCache(const State& s) const {
        return Value<ConfigurationCache>::downcast(
            updCacheEntry(s,configurationCacheIndex)).upd();
    }
    const DynamicsCache& getDynamicsCache(const State& s) const {
        return Value<DynamicsCache>::downcast(
            getCacheEntry(s,dynamicsCacheIndex)).get();
    }
    DynamicsCache& updDynamicsCache(const State& s) const {
        return Value<DynamicsCache>::downcast(
            updCacheEntry(s,dynamicsCacheIndex)).upd();
    }


public:
    TwoPointSpringSubsystemRep(int b1, const Vec3& s1,
                               int b2, const Vec3& s2,
                               const Real& k, const Real& x0)
     : ForceSubsystemRep("TwoPointSpringSubsystem", "0.0.1"), 
       body1(b1), body2(b2), station1(s1), station2(s2),
       defaultParameters(k,x0), built(false)
    {
    }

    // Pure virtuals: TODO??

    const Vec3& getGravity(const State& s) const {return getParameters(s).gravity;}
    Vec3&       updGravity(State& s)       const {return updParameters(s).gravity;}

    const Real& getDamping(const State& s) const {return getParameters(s).damping;}
    Real&       updDamping(State& s)       const {return updParameters(s).damping;}

    const Real& getStiffness(const State& s) const {return getParameters(s).stiffness;}
    Real&       updStiffness(State& s)       const {return updParameters(s).stiffness;}

    const Real& getNaturalLength(const State& s) const {return getParameters(s).naturalLength;}
    Real&       updNaturalLength(State& s)       const {return updParameters(s).naturalLength;}

    const Real& getPotentialEnergy(const State& s) const {return getConfigurationCache(s).pe;}
    const Vec3& getForceOnStation1(const State& s) const {return getDynamicsCache(s).f1_G;}

    void realizeConstruction(State& s) const {
        parameterVarsIndex = s.allocateDiscreteVariable(getMySubsystemIndex(), Stage::Parametrized, 
            new Value<Parameters>(defaultParameters));
        configurationCacheIndex = s.allocateCacheEntry(getMySubsystemIndex(), Stage::Configured,
            new Value<ConfigurationCache>());
        dynamicsCacheIndex = s.allocateCacheEntry(getMySubsystemIndex(), Stage::Dynamics,
            new Value<DynamicsCache>());
        built = true;
    }

    void realizeModeling(State& s) const {
        static const char* loc = "TwoPointSpring::realizeModeling()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Built, loc);
        // Sorry, no choices available at the moment.
    }

    void realizeParameters(const State& s) const {
        static const char* loc = "TwoPointSpring::realizeParameters()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Modeled, loc);
        SimTK_VALUECHECK_NONNEG_ALWAYS(getStiffness(s), "stiffness", loc);
        SimTK_VALUECHECK_NONNEG_ALWAYS(getNaturalLength(s), "naturalLength", loc);

        // Nothing to compute here.
    }

    void realizeTime(const State& s) const {
        static const char* loc = "TwoPointSpring::realizeTime()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Parametrized, loc);
        // Nothing to compute here.
    }

    void realizeConfiguration(const State& s) const {
        static const char* loc = "TwoPointSpring::realizeConfiguration()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Timed, loc);

        const Parameters&   p  = getParameters(s);
        ConfigurationCache& cc = updConfigurationCache(s);

        const MultibodySystem& mbs = getMultibodySystem(); // my owner

        // TODO: handle multiple matter subsystems
        const Transform& X_GB1 = mbs.getMatterSubsystem(0).getBodyConfiguration(s, body1);
        const Transform& X_GB2 = mbs.getMatterSubsystem(0).getBodyConfiguration(s, body2);

        // Fill in the configuration cache.
        cc.station1_G = X_GB1.R() * station1;   // stations expressed in G will be needed later
        cc.station2_G = X_GB2.R() * station2;

        const Vec3 p1_G = X_GB1.T() + cc.station1_G;    // station point locations in ground
        const Vec3 p2_G = X_GB2.T() + cc.station2_G;

        cc.v_G              = p2_G - p1_G;
        cc.x                = cc.v_G.norm();
        const Real stretch  = cc.x - p.naturalLength;   // + -> tension, - -> compression
        cc.fscalar          = p.stiffness * stretch;    // k(x-x0)
        cc.pe               = 0.5 * cc.fscalar * stretch;
    }

    void realizeMotion(const State& s) const {
        static const char* loc = "TwoPointSpring::realizeMotion()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Configured, loc);
        // Nothing to compute here.
    }

    void realizeDynamics(const State& s) const {
        static const char* loc = "TwoPointSpring::realizeDynamics()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Moving, loc);

        const ConfigurationCache& cc = getConfigurationCache(s);
        DynamicsCache&            dc = updDynamicsCache(s);

        dc.f1_G = (cc.fscalar/cc.x) * cc.v_G;  // NaNs if x (and hence v) is 0

        const MultibodySystem& mbs = MultibodySystem::downcast(getSystem());
        const MatterSubsystem& matter = mbs.getMatterSubsystem(0); // TODO: multiple matter subsys
        const int nBodies     = matter.getNBodies();
        const int nParticles  = matter.getNParticles();
        const int nMobilities = matter.getNMobilities();

        // TODO: multiple matter subsys (not "0"!)
        Vector_<SpatialVec>& rigidBodyForces = mbs.getRep().updRigidBodyForces(s,0);
        Vector_<Vec3>&       particleForces  = mbs.getRep().updParticleForces(s,0);
        Vector&              mobilityForces  = mbs.getRep().updMobilityForces(s,0);
        Real&                pe              = mbs.getRep().updPotentialEnergy(s);

        assert(rigidBodyForces.size() == nBodies);
        assert(particleForces.size()  == nParticles);
        assert(mobilityForces.size()  == nMobilities);

        // no particle stuff TODO
        pe += cc.pe;
        rigidBodyForces[body1] += SpatialVec( cc.station1_G % dc.f1_G, dc.f1_G );
        rigidBodyForces[body2] -= SpatialVec( cc.station2_G % dc.f1_G, dc.f1_G );

        if (getDamping(s) != 0)
            mobilityForces -= getDamping(s)*matter.getU(s);
    }

    void realizeReaction(const State& s) const {
        static const char* loc = "TwoPointSpring::realizeReaction()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Dynamics, loc);

        // Nothing to compute here.
    }

    TwoPointSpringSubsystemRep* cloneSubsystemRep() const {return new TwoPointSpringSubsystemRep(*this);}
    friend std::ostream& operator<<(std::ostream& o, 
                         const TwoPointSpringSubsystemRep::Parameters&); 
    friend std::ostream& operator<<(std::ostream& o, 
                         const TwoPointSpringSubsystemRep::ConfigurationCache&); 
    friend std::ostream& operator<<(std::ostream& o, 
                         const TwoPointSpringSubsystemRep::DynamicsCache&);

};
// Useless, but required by Value<T>.
std::ostream& operator<<(std::ostream& o, 
                         const TwoPointSpringSubsystemRep::Parameters&) 
{assert(false);return o;}
std::ostream& operator<<(std::ostream& o, 
                         const TwoPointSpringSubsystemRep::ConfigurationCache&) 
{assert(false);return o;}
std::ostream& operator<<(std::ostream& o, 
                         const TwoPointSpringSubsystemRep::DynamicsCache&) 
{assert(false);return o;}


// 
// Define a uniform gravity field which affects all the matter in the system.
// Parameters exist for the gravity vector, zero height, and enable/disable.
// The MultibodySystem we're part of provides the memory into which we
// accumulate forces and potential energy.
//

class UniformGravitySubsystemRep : public ForceSubsystemRep {

    // State entries. TODO: these should be at a later stage
    // since they can't affect anything until potential energy.
    struct Parameters {
        Parameters() 
          : gravity(0), zeroHeight(0), enabled(true) { }
        Parameters(const Vec3& g, const Real& z) 
          : gravity(g), zeroHeight(z), enabled(true) { }

        Vec3 gravity;
        Real zeroHeight;
        bool enabled;
    };

    struct ParameterCache {
        UnitVec3 gDirection;
        Real     gMagnitude;
    };

    // topological variables
    Parameters defaultParameters;

    // These must be filled in during realizeConstruction and treated
    // as const thereafter. These are garbage unless built=true.
    mutable int parameterVarsIndex;
    mutable int parameterCacheIndex;
    mutable int configurationCacheIndex;
    mutable bool built;

    const Parameters& getParameters(const State& s) const {
        return Value<Parameters>::downcast(
            getDiscreteVariable(s,parameterVarsIndex)).get();
    }
    Parameters& updParameters(State& s) const {
        return Value<Parameters>::downcast(
            updDiscreteVariable(s,parameterVarsIndex)).upd();
    }

    const ParameterCache& getParameterCache(const State& s) const {
        return Value<ParameterCache>::downcast(
            getCacheEntry(s,parameterCacheIndex)).get();
    }
    ParameterCache& updParameterCache(const State& s) const {
        return Value<ParameterCache>::downcast(
            updCacheEntry(s,parameterCacheIndex)).upd();
    }

public:
    UniformGravitySubsystemRep() 
      : ForceSubsystemRep("UniformGravitySubsystem", "0.0.1"), 
        built(false) { } 
    explicit UniformGravitySubsystemRep(const Vec3& g, const Real& z=0)
      : ForceSubsystemRep("UniformGravitySubsystem", "0.0.1"), 
        defaultParameters(g,z), built(false) { }

    // Pure virtuals
    // TODO ??


    const Vec3& getGravity(const State& s) const {return getParameters(s).gravity;}
    Vec3&       updGravity(State& s)       const {return updParameters(s).gravity;}

    const Real& getZeroHeight(const State& s) const {return getParameters(s).zeroHeight;}
    Real&       updZeroHeight(State& s)       const {return updParameters(s).zeroHeight;}

    bool  isEnabled(const State& s) const {return getParameters(s).enabled;}
    bool& updIsEnabled(State& s)    const {return updParameters(s).enabled;}

    // Responses
    const Real& getGravityMagnitude(const State& s) const {
        return getParameterCache(s).gMagnitude;
    }
    const UnitVec3& getGravityDirection(const State& s) const {
        return getParameterCache(s).gDirection;
    }

    void realizeConstruction(State& s) const {
        parameterVarsIndex = s.allocateDiscreteVariable(getMySubsystemIndex(), Stage::Parametrized, 
            new Value<Parameters>(defaultParameters));
        parameterCacheIndex = s.allocateCacheEntry(getMySubsystemIndex(), Stage::Parametrized,
            new Value<ParameterCache>());
        built = true;
    }

    // realizeModeling() not needed

    void realizeParameters(const State& s) const {
        static const char* loc = "UniformGravity::realizeParameters()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Modeled, loc);
        // any values are acceptable

        ParameterCache& pc = updParameterCache(s);

        const Vec3& g = getGravity(s);
        pc.gMagnitude = g.norm();
        pc.gDirection = UnitVec3(pc.gMagnitude==0 ? Vec3(0,0,1) : g/pc.gMagnitude,
                                 true); // this means "trust me; already normalized"
    }

    // realizeTime() not needed

    void realizeConfiguration(const State& s) const {
        static const char* loc = "UniformGravity::realizeConfiguration()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Timed, loc);
        // Nothing to compute here. 
        // TODO: what about pe???
    }

    // realizeMotion() not needed

    void realizeDynamics(const State& s) const {
        static const char* loc = "UniformGravity::realizeDynamics()";
        SimTK_STAGECHECK_GE_ALWAYS(getStage(s), Stage::Moving, loc);

        if (!isEnabled(s) || getGravityMagnitude(s)==0)
            return; // nothing to do

        const Vec3& g   = getGravity(s);  // gravity is non zero
        const Real  gh0 = getGravityMagnitude(s)*getZeroHeight(s); // amount to subtract from gh for pe

        const MultibodySystem& mbs = MultibodySystem::downcast(getSystem());
        for (int msub=0; msub < mbs.getNMatterSubsystems(); ++msub) {
            const MatterSubsystem& matter = mbs.getMatterSubsystem(msub);
            const int nBodies    = matter.getNBodies();
            const int nParticles = matter.getNParticles();

            Vector_<SpatialVec>& rigidBodyForces = mbs.getRep().updRigidBodyForces(s,msub);
            Vector_<Vec3>&       particleForces  = mbs.getRep().updParticleForces(s,msub);
            Real&                pe              = mbs.getRep().updPotentialEnergy(s);

            assert(rigidBodyForces.size() == nBodies);
            assert(particleForces.size() == nParticles);

            if (nParticles) {
                const Real* m = &matter.getParticleMasses(s)[0];
                for (int i=0; i < nParticles; ++i)
                    particleForces[i] += g * m[i];
            }

            for (int i=0; i < nBodies; ++i) {
                const Real&      m     = matter.getBodyMass(s,i);
                const Vec3&      com_B = matter.getBodyCenterOfMass(s,i);
                const Transform& X_GB  = matter.getBodyConfiguration(s,i);
                const Vec3       com_B_G = X_GB.R()*com_B;
                const Vec3       com_G = X_GB.T() + com_B_G;

                pe += m*(~g*com_G - gh0);
                rigidBodyForces[i] += SpatialVec(com_B_G % (m*g), m*g); 
            }
        }
    }

    // realizeReaction() not needed

    UniformGravitySubsystemRep* cloneSubsystemRep() const {return new UniformGravitySubsystemRep(*this);}
    friend std::ostream& operator<<(std::ostream& o, 
                         const UniformGravitySubsystemRep::Parameters&); 
    friend std::ostream& operator<<(std::ostream& o, 
                         const UniformGravitySubsystemRep::ParameterCache&);
};

// Useless, but required by Value<T>.
std::ostream& operator<<(std::ostream& o, 
                         const UniformGravitySubsystemRep::Parameters&) 
{assert(false);return o;}
std::ostream& operator<<(std::ostream& o, 
                         const UniformGravitySubsystemRep::ParameterCache&) 
{assert(false);return o;}


// This is an empty placeholder force subsystem. It does nothing but exist; is that
// really so different from the rest of us?
class EmptyForcesSubsystemRep : public ForceSubsystemRep {
public:
    EmptyForcesSubsystemRep()
      : ForceSubsystemRep("EmptyForcesSubsystem", "0.0.1") { }

    EmptyForcesSubsystemRep* cloneSubsystemRep() const 
      { return new EmptyForcesSubsystemRep(*this); }

    SimTK_DOWNCAST(EmptyForcesSubsystemRep,ForceSubsystemRep);
};

} // namespace SimTK

#endif // SimTK_SIMBODY_FORCES_REP_H_
