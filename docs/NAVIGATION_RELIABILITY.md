# Fleet navigation reliability iteration

Status: implementation and measurement in progress, September 18, 2026.

The deployed contract-2 policies were trained on generator 10 and remain the
published baseline. This iteration uses generator 13, including its coast-shaped
seabed. Do not replace the public policies merely because a training job finishes.

## Acceptance and evaluation

The target is at least 95% collision-free requested arrivals **for every family**
on unseen worlds across three training seeds. Unavailable routes remain in the
denominator and are reported separately. Also record terrain/unit contact
decisions, distinct contact episodes, persistent deadlocks, legitimate yielding,
recovery attempts, mission time and repeated-destination throughput. A ten-second
run without route progress is a stall diagnostic, not by itself proof of deadlock.
Run a continuous browser patrol without respawns or hidden controller changes.

Use generator-13 maps 31001–31008 for diagnosis, 301–332 for training, 32001–32008
for checkpoint selection, and 41001–41016 for final held-out evaluation. Compare
the deployed policy, neutral route tracker and candidate on identical scenarios.
Preserve final-test isolation; do not tune against the held-out results. Record
both the combined navigation system and policy ablations when recovery assists
control. Earlier generator-10 scores are historical, not current-world results.

## Work sequence

1. Pin uv tooling; reproduce existing failures and verify rendered body envelopes,
   collision clearances and route feasibility. Add contact-source diagnostics
   without changing baseline control or rewards.
2. Retain A* global guidance and PPO local decisions. Improve bounded steering,
   sensor-history anticipation, predictive clearance, yielding, physically valid
   retreat and bounded replanning. Never teleport units or turn fixed wings into
   hovering craft. Keep observations tied to actual equipped sensors.
3. Prepare clear-route, head-on, crossing, overtaking, queue, narrow entrance and
   repeated-destination curricula on generated maps. Validate each claimed
   encounter rather than inferring it from a scenario label. Train all five
   family policies together, with independent per-unit recurrence.
4. Mix frozen historical traffic with learners. Frozen actors must not contribute
   mismatched actions or gradients to learner PPO batches. Evaluate unfamiliar
   traffic controllers as well as unfamiliar maps.
5. Run native/WASM contract, allocation and physical-route checks; CUDA gather
   and finite-training checks; three-seed training and held-out evaluation;
   then browser endurance. Publish replacement policies only on measured
   improvement, and disclose any unmet target.

## Self-play decision

Implement historical navigation traffic in this iteration to diversify driving
behavior. Defer competitive faction self-play: combat actions, team objectives
and win/loss outcomes do not exist yet. This is navigation robustness with frozen
traffic, not warfare self-play. Preserve the upstream self-play path and explicit
trainable/frozen actor routing.

All step/reset buffers remain preallocated; expensive route preparation belongs
outside the physics loop or in explicitly bounded, allocation-free recovery work.
Rendering stays optional. Changes to observation/action meaning require a new
contract and explicit checkpoint provenance; retain the old baseline for comparison.
