# Biological warfare and cultivation

Status: accepted creative direction, September 17, 2026. The systems below are
planned; the current release implements terrain, sensing and shared navigation.
The proposed first implementation is [Cultivate and Defend](CULTIVATION_SLICE.md).

## World identity

Humans do not exist in AlienWars. All factions are alien, and all military craft,
weapons, sensors and infrastructure are grown biological organisms or tissues.
Manufacturing-like decisions remain: capacity, production time, specialization,
inventory, maintenance and supply. Their mechanisms are cultivation, breeding,
grafting, nourishment, harvesting and regeneration.

Each unit is itself the biological entity. There are no pilots, crew, riders,
occupants or separate creature/vehicle pairing. Use unit/entity, body and organ
language for new content. An RL policy is the software decision function for an
entity, not an inhabitant inside it. A caretaker is also a biological entity.

This is the basis of the economy and physical behavior as well as the art.
Nurseries are living production sites; equipment is a specialized organ or
graft; a deployed craft has a body, metabolism and nutritional requirements.
Existing vehicle models are development stand-ins. Preserve their established
movement families and truthful historical training records while introducing
biological models and names for new content.

## Cultivation loop

**Prepare habitat → seed → nurture → mature → harvest or awaken → sustain → recycle.**

| Stage | Decision and consequence |
| --- | --- |
| Prepare | Enrich soil or condition water, establish a viable growing bed and provide caretaker access. Habitat determines compatible organisms. |
| Seed | Commit a propagule, nutrients, space and growing time to a crop or craft. |
| Nurture | Supply nourishment and care; allocate limited caretakers between competing beds and protect their routes. |
| Mature | Deploy early or keep investing in a larger, more capable organism. Choose specialized growth branches where supported. |
| Collect | Harvest ammunition pods, nutrient reserves or protective tissue; awaken mobile organisms at a viable exit. |
| Sustain | Replenish reserves, repair tissue and support deployed organisms through supply access. |
| Recycle | Recover a bounded fraction of remaining biomass from spent crops and casualties. Recovery takes work and cannot generate resources through repeated grow/recycle cycles. |

The future army is present on the map while it grows. Scouting nurseries can
reveal likely capabilities. Interrupting supply, threatening a bed or forcing
early deployment gives attack and defense objectives beyond destroying units.

Start with three cultivation variables: **nutrient availability, moisture and
habitat compatibility**. Keep global inventory separate from local bed condition;
moisture or a favorable habitat does not create free nutrients. Soil and water
preparation must change growth eligibility or rate. Caretakers automate routine
work after receiving priorities, while a player or future commander policy
chooses sites, investments, specialization and deployment timing.

Soil beds support terrestrial organisms. Submerged nurseries support aquatic
organisms. Specialized nests can later support flight organisms. Temperature,
salinity, disease, inheritance and complex chemistry are expansion candidates,
not required parts of the initial economy.

## Bodies, equipment and combat

Use shared physical budgets rather than unrelated bonuses for every item.

| Budget or mechanism | Biological expression and tradeoff |
| --- | --- |
| Mass and inertia | Carapace, organs and reserves alter acceleration, braking, turning and recoil response. Greater mass can support a more stable firing platform. |
| Metabolic power | Propulsion, sensing, weapons and repair compete for available output. |
| Heat and stress | Sustained activity limits firing and movement until the organism recovers. |
| Ammunition | Harvested pods or internal reserves have finite capacity and explicit replenishment costs. |
| Armor | Protective tissue increases survival while consuming mass and growth investment; directional protection rewards positioning. |
| Recoil and stabilization | Firing perturbs the body and aim; stance, stabilizing limbs and body mass affect recovery. |
| Regeneration | Healing consumes nourishment and time, with an opportunity cost against other functions. |

Growth should produce visible and mechanical differences: early deployment
offers mobility and lower investment; continued maturation can buy payload,
protection or endurance. Specializing sensory organs competes with weapon and
reserve capacity. A heavier body can also lose access to narrow passages or
shallow water, so navigation must use the same body and movement limits.

Separate delivery behavior from payload effect. Candidate effects include
penetration, fragmentation and temporary sensory disruption; candidate delivery
behaviors include direct projectiles and guided organisms. Every implemented
combination needs explicit medium, range, collision, friendly-fire and counterplay
rules. These are fictional game abstractions, not biological engineering models.
Begin with a small ammunition set before multiplying weapon combinations.

Defense includes cover, facing, evasion, protecting supply and retreating to
recover. Later defensive organs can add shields or countermeasures with explicit
metabolic costs. A mobile organism's role should follow its locomotion: forward
flight requires attack passes, hovering supports holding angles, and aquatic
movement makes water access strategically valuable.

## Perception and terrain

The policy acts on its own sensor-derived contacts and remembered information.
Spectator see-through, sensor overlays and selection indicators cannot reveal
hidden targets to an actor or change simulation outcomes. Biological sensory
organs may reuse the existing geometric sensing channels with clearly documented
game semantics; current channels must not silently gain new abilities.

Terrain, tunnels, bridges and water remain authoritative for movement, sensing
and projectile obstruction. Any prop used as cover needs an authoritative body
shared by these systems. Current decorative trees and rocks are not cover.
Growth sites must fit the generated terrain and provide reachable care and
deployment routes. Preserve procedural variety and declared symmetry.

## Species and factions

Define species through ecology, body structure and cultivation methods together.
Possible directions include soil/root colonies, tidal nursery civilizations and
species that raise airborne organisms in brood structures. All use biological
craft; an industrial-looking silhouette does not imply manufactured machinery.

Species names, number and detailed traits are not finalized. Earlier names and
mineral/machine concepts were brainstorming, not approved canon. Start with one
species and two opposing colonies to validate the common rules. Later species
should create different economic and tactical decisions, not just damage
multipliers. A species can contain multiple political factions.

Keep the established restrained, realistic RTS presentation: readable body
orientation, distinct roles, visible growth and impacts, subdued terrain, and
effects that communicate state. Avoid requiring exaggerated colors or constant
glow to distinguish organisms.

## Learning direction

Retain A* for global routing and PPO for local control. Changes to mass, size,
equipment and damage require explicit observations and movement validation;
current navigation checkpoints are not presumed competent under new physics.

Stage learning from movement under biological loadouts, through engagement and
defense, toward squad tactics and cultivation decisions. Start with automated
caretaker jobs and explicit priorities. Learning long-horizon economic planning
can follow a validated cultivation loop and competent combat controllers.

Team victory and objective defense should drive combat evaluation. Damage,
survival and resource use provide supporting measurements; farming damage or
avoiding battle indefinitely must not masquerade as winning. Competitive
self-play is intended once the scenario has defined observations, actions,
rewards and win/loss/draw rules. See the
[self-play direction](SHARED_NAVIGATION.md#self-play-and-faction-warfare).

Use the shared renderer-independent C simulation with fixed buffers and no
step/reset allocations. Raylib/WASM should display the same authoritative growth,
movement and combat states used by headless PufferLib training. Measure
throughput as entity counts and interaction complexity increase.

## Proposed data architecture

Use data-driven composition with a bounded Entity Component System in C. The
current fleet now uses [Flecs 4.1.6](FLECS.md) for live unit storage and lifecycle.
The broader biological definitions, growth and combat components below remain
proposed extensions; the port preserves the current navigation behavior.

- **Definitions:** versioned, validated authoring tables describe biological
  forms, organs, growth recipes, habitat requirements and payloads. Resolve names
  to IDs and bake them into immutable C tables shared by native and WASM builds.
  Validate units, capacities, references and allowed combinations before runtime.
  Shared systems implement rules; definitions supply their parameters.
- **Identity:** each entity has a stable slot and generation counter. Components
  describe body, locomotion, metabolism, senses, growth, inventory or weapon
  organs as needed. Ordinary organs can be bounded component records within their
  owner; model them as separate entities only if independent targeting, attachment
  or lifecycle requires it. Do not model every cell as an entity.
- **Storage:** keep frequently processed fields in compact component arrays,
  using structure-of-arrays for suitable hot loops. Small coupled records can
  remain structs. Preallocate to scenario-specific capacities, with capability
  masks and active lists; do not allocate every optional component for every
  possible entity. Definitions and immutable map data may be shared; each world
  owns its mutable state and RNG. Profile layout choices rather than assuming an
  ECS or structure-of-arrays automatically improves throughput.
- **Systems:** growth, supply, metabolism, locomotion, sensing and combat operate
  over their relevant components. Declare read/write order and simulated cadence.
  Queue structural changes in bounded buffers and commit them at defined phase
  boundaries so birth, death and harvesting cannot invalidate iteration. Resolve
  simultaneous interactions without giving entity storage order a gameplay edge.
- **Rendering:** biological meshes, animation, particles and UI consume snapshots
  or events. They cannot supply authoritative positions, damage or growth state.
  Headless rollouts omit presentation resources and work.
- **RL views:** construct versioned, fixed-shape observations from the state an
  actor is allowed to know: self state, sensed contacts, nearby relevant habitat
  and task guidance. Bound contact slots, retain stable track correspondence and
  explicit validity/age, and document overflow selection and its information
  loss. Keep hidden world state out of actor inputs. Flatten this view into the
  trainer's contiguous buffers; do not serialize the whole entity world per step.

Simulation entity IDs, actor slots and family/faction policy assignments are
separate mappings. A bed, inventory store or projectile does not need an
independent PPO policy merely because it is an entity. Keep bounded actor slots
for trainable units, even when live unit counts change.

The current `Agent` interface has observations, actions, rewards, terminals,
action masks and policy assignment; it does not expose a general sample-validity
field for dynamic births. The combat adapter/trainer must explicitly handle this:
retain the valid transition that kills a unit, end its recurrent/return sequence,
exclude subsequent empty-slot padding from losses and advantage statistics, and
reset state before a new birth. An illegal-action mask alone does not exclude
padding from PPO training. Frozen opponents also require explicit trainability
routing. Reused slots must not inherit targets, observations or memory.

Measure bytes per environment and rollout, sensor/query cost, observation packing,
CPU/GPU transfer and learner throughput independently. CPU simulation with GPU
PPO remains the initial path. Component arrays make later batching practical,
but a GPU environment requires its own measured, validated implementation.

The [Flecs design guide](https://www.flecs.dev/flecs/DesignWithFlecs.html) and
[storage FAQ](https://www.flecs.dev/flecs/FAQ.html) inform these patterns. Use our
[runtime validation](FLECS.md#validation) for evidence about AlienWars behavior
and costs rather than inferring gains from general ECS benchmarks.
