# Next development slice: Cultivate and Defend

Status: proposed implementation slice, September 17, 2026. Documentation only;
no cultivation, combat or new trained behavior is implemented by this plan.
It follows the accepted [biological warfare direction](BIOLOGICAL_WARFARE.md).
Roster, timing and capacity below are initial design choices to validate.

Every unit is the biological entity itself, with no pilot, crew, occupant or
separate creature/vehicle pairing. The caretaker, combat bodies and colony core
are all biological entities.

## Playable outcome

On a procedural coastal map, prepare soil and water nurseries, seed organisms,
allocate care, collect ammunition and awaken a small force. Protect a living
colony core while attacking the opposing colony. Early deployment trades
strength and ammunition capacity for time; sustained fighting consumes harvested
pods and creates opportunities to retreat or raid supplies.

The slice is complete when this entire loop works interactively in the browser
and reproducibly in headless C, with disclosed reference opponents. PPO combat
training is a subsequent milestone after the environment passes its checks.

## Bounded content

Use one provisional species and two colonies with identical capabilities.
Each colony starts with a core, a caretaker, seed stock and a finite nutrient
reserve. Provide one soil bed and one sea-connected water nursery per colony,
with validated caretaker access, deployment space and combat access between sides.

| Organism or structure | First-slice role |
| --- | --- |
| Colony core | Stores nutrients, seeds and harvested pods; losing it loses the round. |
| Caretaker | Uses existing ground navigation to prepare beds, deliver nourishment and collect crops. A reachable shore interaction point serves the water nursery. |
| Ground combat organism | Grows in soil; maneuvers, aims and fires a direct pod projectile. Can brace while stationary to reduce recoil and aim disturbance. |
| Surface-water combat organism | Grows in water; obeys draft and turn limits, carries the same ammunition class and can attack exposed shoreline targets. |
| Ammunition crop | Occupies a prepared soil bed, consumes nutrients and yields a finite batch of pods when collected. Competes with ground-unit production for the bed. |

Working roles are not final species or unit names. Soil and water combat bodies
need distinct grown silhouettes, visible forward direction and physical sensor
mounts. Both early and mature deployments must be visually distinguishable.
The existing fleet and its documented navigation results remain available as a
separate experiment; the slice does not require replacing all twelve models.

One bed holds one crop or developing organism at a time. Initial deployments are
empty of ammunition and require a harvested supply, making collection functional.
Use a small declared per-team entity cap (initial target: eight mobile organisms)
and bounded projectile/storage pools. A full pool never consumes resources for a
spawn that did not happen, and cannot silently overwrite a live entity.

## Cultivation and supply rules

1. Validate habitat, access and free growing space before preparing a bed.
   Preparation consumes caretaker time and inventory; it establishes suitable
   soil moisture or a conditioned water nursery.
2. Seeding reserves the bed and atomically spends one seed and the configured
   establishment nutrients. Invalid or repeated commands spend nothing.
3. Growth requires compatible habitat, adequate moisture and caretaker-delivered
   nutrients. Shortage pauses progress with a visible cause; waiting alone does
   not create biomass. The water nursery uses a saturated moisture condition
   and requires conditioning and nourishment rather than irrigation.
4. At a minimum viable stage, a combat organism may awaken early. Further growth
   costs additional nutrients and care, producing a heavier, better protected
   body with a larger pod capacity. Apply the same mass-dependent movement and
   recoil rules to both stages. Freeze developmental stage after awakening for
   this slice; runtime weight still reflects carried inventory.
5. A caretaker physically collects ripe pods and delivers them to storage.
   Reloading transfers real inventory at a reachable core or shoreline supply
   point. No remote ammunition replenishment.
6. Spent beds require preparation before reuse. Cancellation and destruction
   produce no automatic full refund. Caretakers can recover a configured fraction
   of remaining biomass; cap it below the net nutrient investment, including any
   ammunition already harvested. Seed stock does not regenerate in this slice.

Finite initial nutrients and seeds bound the first match economy. Ongoing
nutrient crops, breeding, seed replenishment and territorial resource extraction
follow later. The UI must show limiting stocks and stalled work clearly.

## Combat and match contract

Start with one direct projectile and one pod ammunition type. Simulate finite
travel time, swept collision against shared terrain and body proxies, damage,
finite magazines, reloads, metabolic stress recovery and recoil. Mounted aim has
a bounded turn rate and firing arc. Ground bracing trades movement for stability;
water organisms remain subject to their normal movement limits. Cosmetic effects
read these events and cannot determine hits or damage.

Use body integrity and directional carapace protection. Front protection is
stronger than rear protection; damage comes from the resolved impact direction.
Pods can hit allied bodies, and terrain blocks shots. Resolve hits from the same
simulation interval without array-order advantages. The first projectile travels
through air and stops at water entry; it cannot attack submerged targets.

Use existing terrain as cover. Add authoritative bodies for the new core and
nurseries to movement, sensing and projectile queries. Decorative trees and rocks
remain excluded from cover claims. No destructible terrain is needed.

Both sides defend a core and can attack the other. Initial round length is
180 simulated seconds. One destroyed core gives the other side a win; both
destroyed in the same resolved interval give a draw; two surviving cores at the
time limit give a draw. Damage totals cannot break a draw. Unit death persists
until a newly grown organism replaces it; arrival does not terminate a combat
unit. Starvation pauses cultivation, not the whole simulation. Reset restores
initial inventories, beds, entities, contacts and controller state.

Begin with validated symmetric coastal scenarios on existing generated terrain;
include at least three different world seeds. Place gameplay sites in a separate
seeded scenario pass without changing terrain hashes. An unsuitable world is an
explicit unavailable scenario, not a terrain edit to force a nursery. Report
availability separately from match outcomes. Asymmetric evaluation follows once
the baseline loop is correct.

## Controls, perception and viewing

Expose prepare, seed, tend priority, collect, awaken early/mature, move, attack,
brace and retreat/reload commands with validity and cost feedback. Automate
caretaker routing and repetitive tending after priorities are assigned. Use a
small deterministic reference opponent that cultivates, reloads, advances and
defends; label it as reference control.

Actors receive local body/resource state and their valid sensor contacts.
Maintain explicit age and validity for remembered contacts. Initial enemy
tracking is per unit; no automatic perfect team vision. Area shots can target
remembered locations, but their aim does not follow an unseen moving target.
Spectator inspection can reveal the whole battle without changing actor inputs.

Keep the charcoal, compact monospace UI. Selecting a bed shows habitat, invested
resources, growth stage, care and the cost of continued maturation. Selecting a
combat organism shows integrity, mass, pods, stress, stance and sensed targets.
Visible growth, body posture and impacts should make the loop readable without
opening diagnostics. Preserve pan/orbit controls and tunnel/sensor inspection.

## Engineering and learning boundary

Follow the proposed [data architecture](BIOLOGICAL_WARFARE.md#proposed-data-architecture):
validated immutable definitions, component-based live state, explicit systems and
a separate fixed-shape RL observation view. Start with the components required
by this slice on the [current Flecs runtime](FLECS.md). A GPU environment is not
a prerequisite; the initial simulation remains C on the CPU with CUDA PPO.

Implement cultivation, inventory and combat as renderer-independent C systems
over the existing terrain, vehicle and sensor contracts. Keep stable entity slots
with generation IDs so death and regrowth cannot reuse stale targets or memory.
Preallocate beds, entities, contact tracks, jobs and projectiles. Match stepping
and reset must allocate nothing. Keep the existing 30 Hz physical timestep and
10 Hz control schedule; use integer simulation ticks for growth and cooldowns.

Mass changes must affect the movement model, not just a displayed statistic.
Mature dimensions and draft must update authoritative collision and route
feasibility. Validate reachable spawn/exit space before deployment. Define blocked
jobs and route retry behavior so a failed caretaker path cannot award deliveries.

Record the new observation/action schema before building an RL adapter: include
body budgets, stance, inventory and contact validity, with explicit unavailable
action masks or defined no-op behavior. Version the combat contract independently
from the existing 645-input navigation contract. Reuse checkpoint weights only
after establishing compatibility and evaluating changed movement; new body state,
death, birth and target selection cannot be silently added to old checkpoints.

**Self-play decision for this slice: defer training until the environment and
reference opponents are validated.** Team identity, victory rules and deterministic
reset support that next step. Then define team reward and any bounded shaping,
evaluate against frozen references, and integrate current/historical opponent
pools with explicit family/faction routing. Keep per-unit recurrent state and
birth/death masks. Training cultivation decisions is a later layer; caretaker
priorities can remain scripted while combat control is learned.

Before training dynamic populations, add explicit valid-transition handling to
the adapter and learner: retain death transitions, exclude empty-slot padding
from losses and advantage normalization, and prevent return/recurrent sequences
from crossing into another entity's life. Action masks alone are insufficient.

## Completion checks

- Demonstrate prepare → seed → tend → awaken/harvest → reload → fight for both
  colonies, including soil and water production, across the declared seed set.
- Verify accounting through duplicate/invalid commands, cancellation, death,
  collection, reloading, full pools and recycling. No negative stock, duplicate
  harvest or resource-positive recycling loop is permitted.
- Show a reproducible early/mature tradeoff and a brace/move tradeoff using
  measured acceleration, turning, recoil displacement and aim recovery. Neither
  maturity stage should dominate every tested situation.
- Test frontal/rear hits, friendly obstruction, terrain cover, water entry,
  fast-projectile collision, simultaneous deaths and every win/loss/draw case.
- Confirm growth deadlines and combat outcomes are independent of rendering
  frame rate; compare native/WASM state traces under documented numeric tolerance.
- Toggle see-through and every overlay while replaying identical commands; actor
  observations and authoritative outcomes must be unchanged.
- Test blocked caretakers, unreachable deployment, dead-target invalidation and
  repeated match resets; run existing navigation and sensing regressions.
- Record headless step throughput against the current navigation baseline, plus
  browser frame time at declared entity/projectile caps. Report actual costs
  before increasing the caps or starting large training runs.
- Inspect the complete loop in Chrome and publish measured limitations alongside
  any eventual release. Reference behavior is not described as learned combat.

## Later slices

Additional species, flying and submerged combat bodies, more payloads, defensive
organs, regeneration, persistent supply networks, nutrient farming, breeding,
learned economic planning and faction self-play expand the validated core loop.
Their absence does not prevent completing this first slice.
