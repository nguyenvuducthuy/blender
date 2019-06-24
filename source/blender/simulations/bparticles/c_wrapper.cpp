#include "BParticles.h"
#include "core.hpp"
#include "particles_container.hpp"
#include "emitters.hpp"
#include "forces.hpp"
#include "events.hpp"
#include "actions.hpp"
#include "simulate.hpp"

#include "BLI_timeit.hpp"
#include "BLI_listbase.h"

#include "BKE_curve.h"
#include "BKE_bvhutils.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

using namespace BParticles;

using BLI::ArrayRef;
using BLI::float3;
using BLI::SmallVector;
using BLI::StringRef;

WRAPPERS(ParticlesState *, BParticlesState);

/* New Functions
 *********************************************************/

BParticlesState BParticles_new_empty_state()
{
  ParticlesState *state = new ParticlesState();
  return wrap(state);
}

void BParticles_state_free(BParticlesState state)
{
  delete unwrap(state);
}

class ModifierStepParticleInfluences : public ParticleInfluences {
 public:
  SmallVector<Force *> m_forces;
  SmallVector<Event *> m_events;
  SmallVector<Action *> m_actions;

  ~ModifierStepParticleInfluences()
  {
    for (Force *force : m_forces) {
      delete force;
    }
    for (Event *event : m_events) {
      delete event;
    }
    for (Action *action : m_actions) {
      delete action;
    }
  }

  ArrayRef<Force *> forces() override
  {
    return m_forces;
  }
  ArrayRef<Event *> events() override
  {
    return m_events;
  }
  ArrayRef<Action *> action_per_event() override
  {
    return m_actions;
  }
};

class ModifierParticleType : public ParticleType {
 public:
  SmallVector<Emitter *> m_emitters;
  ModifierStepParticleInfluences m_influences;

  ~ModifierParticleType()
  {
    for (Emitter *emitter : m_emitters) {
      delete emitter;
    }
  }

  ArrayRef<Emitter *> emitters() override
  {
    return m_emitters;
  }

  ParticleInfluences &influences() override
  {
    return m_influences;
  }
};

class ModifierStepDescription : public StepDescription {
 public:
  float m_duration;
  SmallMap<uint, ModifierParticleType *> m_types;

  ~ModifierStepDescription()
  {
    for (auto *type : m_types.values()) {
      delete type;
    }
  }

  float step_duration() override
  {
    return m_duration;
  }

  ArrayRef<uint> particle_type_ids() override
  {
    return {0};
  }

  ParticleType &particle_type(uint type_id) override
  {
    return *m_types.lookup(type_id);
  }
};

void BParticles_simulate_modifier(NodeParticlesModifierData *npmd,
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState state_c)
{
  SCOPED_TIMER("simulate");

  ParticlesState &state = *unwrap(state_c);
  ModifierStepDescription description;
  description.m_duration = 1.0f / 24.0f;

  auto *type = new ModifierParticleType();
  description.m_types.add_new(0, type);

  if (npmd->emitter_object) {
    type->m_emitters.append(EMITTER_mesh_surface((Mesh *)npmd->emitter_object->data,
                                                 npmd->emitter_object->obmat,
                                                 npmd->control1)
                                .release());
  }
  BVHTreeFromMesh treedata = {0};
  if (npmd->collision_object) {
    BKE_bvhtree_from_mesh_get(
        &treedata, (Mesh *)npmd->collision_object->data, BVHTREE_FROM_LOOPTRI, 4);
    type->m_influences.m_events.append(
        EVENT_mesh_collection(&treedata, npmd->collision_object->obmat).release());
    type->m_influences.m_actions.append(ACTION_kill().release());
  }
  type->m_influences.m_forces.append(FORCE_directional({0, 0, -2}).release());
  type->m_influences.m_events.append(EVENT_age_reached(3.0f).release());
  type->m_influences.m_actions.append(ACTION_move({0, 1, 0}).release());
  simulate_step(state, description);

  if (npmd->collision_object) {
    free_bvhtree_from_mesh(&treedata);
  }

  auto &containers = state.particle_containers();
  for (auto item : containers.items()) {
    std::cout << "Particle Type: " << item.key << "\n";
    std::cout << "  Particles: " << item.value->count_active() << "\n";
    std::cout << "  Blocks: " << item.value->active_blocks().size() << "\n";
  }
}

uint BParticles_state_particle_count(BParticlesState state_c)
{
  ParticlesState &state = *unwrap(state_c);

  uint count = 0;
  for (ParticlesContainer *container : state.particle_containers().values()) {
    count += container->count_active();
  }
  return count;
}

void BParticles_state_get_positions(BParticlesState state_c, float (*dst_c)[3])
{
  ParticlesState &state = *unwrap(state_c);
  float3 *dst = (float3 *)dst_c;

  uint index = 0;
  for (ParticlesContainer *container : state.particle_containers().values()) {
    for (auto *block : container->active_blocks()) {
      auto positions = block->slice_active().get_float3("Position");
      positions.copy_to(dst + index);
      index += positions.size();
    }
  }
}