#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Frame-level simulation of the TSN network modelled in src/network.c, to
check that observed delays never exceed the Network Calculus bounds.

Input: a file written by net_export: a "#net" line with the network, the
rate of every link ("#rate"), then one flow per row with its class and its
route (the ECUs are the first nodes, the switches follow).

Every output port of every route is simulated at once (event driven, so
cyclic routes on a ring are fine). A port serves its LET classes in strict
priority, either through one credit-based shaper per class (IEEE 802.1Qav,
non-preemptive) or through a cyclic gate schedule (IEEE 802.1Qbv: one
exclusive window per class, then the best-effort window; a frame only
starts if it ends inside its window), or through asynchronous traffic
shaping (IEEE 802.1Qcr: strict priority, and at every switch a per-flow
token-bucket regulator set to the arrival curve of the flow at its
source, b + r J and r). With frame replication (IEEE 802.1CB), a flow
with a second route is replicated by the switch of its source onto both
routes and the switch of its destination keeps the first copy of each
frame (after the regulators of ATS, which act on each member stream).
The best-effort class is either
saturated with maximum size frames (worst blocking) or absent. A switch
forwards a frame after it is fully received (store and forward) plus a
constant latency.

usage: netsim.py <flows.csv> <horizon us> <runs>
"""
import csv
import heapq
import random
import sys
from collections import defaultdict, deque

L_BE = (1500 + 42) * 8      # best-effort max frame on the wire, bits
EPS = 1e-9
CREDIT_EPS = 1e-6           # bits: credits are compared to 0 within this


def frames_of(payload):
    left, out = max(payload, 1), []
    while left > 0:
        p = min(left, 1500)
        out.append((max(p, 42) + 42) * 8)
        left -= p
    return out


class Port:
    def __init__(self, sim, rate, slopes):
        self.sim = sim
        self.rate = rate
        k = sim.classes
        self.queues = [deque() for _ in range(k)]
        self.credit = [0.0] * k
        # idle slopes of the analysis at this port, else the shares
        self.slope = slopes or [rate * s / 100.0 for s in sim.shares]
        self.busy = None            # (class or None for best effort)
        self.last = 0.0

    def update(self, t):
        """credits over [last, t] (CBS)"""
        dt = t - self.last
        if dt > 0 and self.sim.shaping == "CBS":
            for k in range(self.sim.classes):
                if self.busy == ("let", k):
                    self.credit[k] += (self.slope[k] - self.rate) * dt
                elif self.queues[k]:
                    self.credit[k] += self.slope[k] * dt
                elif self.credit[k] < 0:
                    self.credit[k] = min(0.0, self.credit[k] +
                                         self.slope[k] * dt)
                else:
                    self.credit[k] = 0.0
        self.last = t

    def start(self, t, what, duration):
        self.busy = what
        self.sim.push(t + duration, "end", self)

    def try_start(self, t):
        if self.busy is not None:
            return
        self.update(t)
        sim = self.sim
        if sim.shaping == "CBS":
            for k in range(sim.classes):
                if self.queues[k] and self.credit[k] >= -CREDIT_EPS:
                    frame = self.queues[k][0]
                    self.start(t, ("let", k), frame[0] / self.rate)
                    return
            if sim.be_sat:
                self.start(t, ("be",), L_BE / self.rate)
                return
            # idle: wake when a blocked class gets its credit back
            for k in range(sim.classes):
                if self.queues[k] and self.credit[k] < 0:
                    # strictly later: rounding must not stall the clock
                    sim.push(t + max(-self.credit[k] / self.slope[k], 1e-7),
                             "wake", self)
            return
        if sim.shaping == "ATS":
            # strict priority; the regulators act before the queues
            for k in range(sim.classes):
                if self.queues[k]:
                    frame = self.queues[k][0]
                    self.start(t, ("let", k), frame[0] / self.rate)
                    return
            if sim.be_sat:
                self.start(t, ("be",), L_BE / self.rate)
            return
        # TAS
        phase = t % sim.cycle
        base = t - phase
        for k in range(sim.classes):
            start, end = sim.windows[k]
            if start - EPS <= phase < end and self.queues[k]:
                frame = self.queues[k][0]
                if phase + frame[0] / self.rate <= end + EPS:
                    self.start(t, ("let", k), frame[0] / self.rate)
                    return
        be_start = sim.windows[-1][1]
        if sim.be_sat and phase >= be_start - EPS and \
                phase + L_BE / self.rate <= sim.cycle + EPS:
            self.start(t, ("be",), L_BE / self.rate)
            return
        # wait for the next window boundary
        for start, end in sim.windows + [(be_start, sim.cycle)]:
            for edge in (start, end):
                if edge > phase + EPS:
                    sim.push(base + edge, "wake", self)
                    return
        sim.push(base + sim.cycle, "wake", self)

    def end(self, t):
        self.update(t)
        what, self.busy = self.busy, None
        if what[0] == "let":
            frame = self.queues[what[1]].popleft()
            if self.sim.shaping == "CBS" and not self.queues[what[1]] \
                    and self.credit[what[1]] > 0:
                self.credit[what[1]] = 0.0
            self.sim.forward(t, frame)
        self.try_start(t)


# a frame: (bits, flow, message, hop, class, member, index in message)


class Simulation:
    def __init__(self, header, slopes, rates, flows, be_sat, rng):
        self.shaping = header["shaping"]
        self.classes = int(header["classes"])
        self.shares = [float(s) for s in header["shares"].split(",")]
        self.cycle = float(header["cycle"])
        self.switch_latency = float(header["switch"])
        offset, self.windows = 0.0, []
        for share in self.shares:
            width = self.cycle * share / 100.0
            self.windows.append((offset, offset + width))
            offset += width
        self.be_sat = be_sat
        self.rng = rng
        self.flows = flows
        self.events, self.seq = [], 0
        self.ports = {}
        for f in flows:
            for a, b in list(zip(f["route"], f["route"][1:])) + \
                    list(zip(f["route2"], f["route2"][1:])):
                if (a, b) not in self.ports:
                    self.ports[(a, b)] = Port(self, rates[(a, b)],
                                              slopes.get((a, b)))
        self.delivered = defaultdict(float)
        self.regulators = {}        # ATS: (port, flow, member) -> state
        self.eliminated = set()     # FRER: (flow, message, index) kept

    def regulate(self, t, port, frame):
        """ATS: time at which the regulator of the flow at this switch
        lets the frame into the port queue (FIFO per flow)"""
        f = self.flows[frame[1]]
        bits = sum(frames_of(f["bytes"]))
        rate = bits / f["T"]
        burst = bits + rate * f["J"]
        state = self.regulators.setdefault((port, frame[1], frame[5]),
                                           [burst, 0.0, 0.0])
        te = max(t, state[2])
        tokens = min(burst, state[0] + rate * (te - state[1]))
        if tokens < frame[0] - EPS:
            te += (frame[0] - tokens) / rate
            tokens = frame[0]
        state[0], state[1], state[2] = tokens - frame[0], te, te
        return te

    def push(self, t, kind, data):
        self.seq += 1
        heapq.heappush(self.events, (t, self.seq, kind, data))

    def route_of(self, frame):
        f = self.flows[frame[1]]
        return f["route2"] if frame[5] else f["route"]

    def forward(self, t, frame):
        bits, flow, message, hop, cls, member, index = frame
        route = self.route_of(frame)
        if hop + 2 == len(route):
            self.in_flight -= 1
            key = (flow, message)
            self.delivered[key] = max(self.delivered[key], t)
            return
        members = [member]
        if hop == 0 and self.flows[flow]["route2"]:
            members = [0, 1]        # replicated by the source switch
            self.in_flight += 1
        for m in members:
            r = self.flows[flow]["route2"] if m else route
            nxt = self.ports[(r[hop + 1], r[hop + 2])]
            self.push(t + self.switch_latency, "arrive",
                      (nxt, (bits, flow, message, hop + 1, cls, m, index)))

    def eliminate(self, frame):
        """FRER: whether a copy reaching the port to its destination is
        dropped (another copy of the frame got there first)"""
        if not self.flows[frame[1]]["route2"] or \
                frame[3] + 2 != len(self.route_of(frame)):
            return False
        key = (frame[1], frame[2], frame[6])
        if key in self.eliminated:
            self.in_flight -= 1
            return True
        self.eliminated.add(key)
        return False

    def run(self, releases):
        self.in_flight = 0
        for flow, message, r in releases:
            f = self.flows[flow]
            port = self.ports[(f["route"][0], f["route"][1])]
            for index, bits in enumerate(frames_of(f["bytes"])):
                self.in_flight += 1
                self.push(r, "arrive",
                          (port, (bits, flow, message, 0, f["class"], 0,
                                  index)))
        if self.be_sat and self.shaping in ("CBS", "ATS"):
            # a best-effort frame in progress at time 0, random phase
            for port in self.ports.values():
                port.start(-self.rng.uniform(0, L_BE / port.rate), ("be",),
                           L_BE / port.rate)
        # until every LET frame is delivered (best effort never ends)
        while self.events and self.in_flight:
            t, _, kind, data = heapq.heappop(self.events)
            if kind == "arrive" and self.shaping == "ATS" and frame_hop(data):
                port, frame = data
                te = self.regulate(t, port, frame)
                self.push(te, "eligible", data)
            elif kind in ("arrive", "eligible"):
                port, frame = data
                if self.eliminate(frame):
                    continue
                port.update(t)
                port.queues[frame[4]].append(frame)
                port.try_start(t)
            elif kind == "end":
                data.end(t)
            else:
                data.try_start(t)


def frame_hop(data):
    """whether a frame arrives at a switch (not at its source port)"""
    return data[1][3] > 0


def releases_of(flows, horizon, offsets, jitter, rng):
    out = []
    for k, f in enumerate(flows):
        T, J = f["T"], f["J"]
        o = 0.0 if offsets == "sync" else rng.uniform(0, T)
        j = 0
        while True:
            if jitter == "uniform":
                u = rng.uniform(0, J)
            elif jitter == "extreme":
                u = rng.choice([0.0, float(J)])
            else:                           # alternate late, early
                u = float(J) if j % 2 == 0 else 0.0
            r = o + j * T + u
            if r > horizon:
                break
            out.append((k, j, r))
            j += 1
    return out


def main():
    path, horizon, runs = sys.argv[1], float(sys.argv[2]), int(sys.argv[3])
    with open(path) as fh:
        first = fh.readline().split()
        header = dict(item.split("=") for item in first[1:])
        slopes, rates, lines = {}, {}, []
        for line in fh:
            if line.startswith("#rate"):
                _, link, value = line.split()
                a, b = (int(n) for n in link.split(">"))
                rates[(a, b)] = float(value)
            elif line.startswith("#slope"):
                _, link, values = line.split()
                a, b = (int(n) for n in link.split(">"))
                slopes[(a, b)] = [float(v) for v in values.split(",")]
            else:
                lines.append(line)
        flows = []
        for row in csv.DictReader(lines):
            flows.append({"bytes": int(row["bytes"]), "T": int(row["T"]),
                          "J": int(row["J"]), "class": int(row["class"]),
                          "route": [int(n) for n in row["route"].split(">")],
                          "route2": [int(n) for n in
                                     row["route2"].split(">")]
                          if row.get("route2") else [],
                          "bound": float(row["bound"])})
    rng = random.Random(1)
    worst_ratio, violations, scenarios = 0.0, 0, 0
    for run in range(runs):
        for be_sat in (True, False):
            for offsets in ("sync", "random"):
                for jitter in ("uniform", "extreme", "alternate"):
                    scenarios += 1
                    rel = releases_of(flows, horizon, offsets, jitter, rng)
                    sim = Simulation(header, slopes, rates, flows, be_sat, rng)
                    sim.run(rel)
                    released = {(k, j): r for k, j, r in rel}
                    worst = defaultdict(float)
                    for (k, j), d in sim.delivered.items():
                        worst[k] = max(worst[k], d - released[(k, j)])
                    for k, d in worst.items():
                        b = flows[k]["bound"]
                        if b < 0:
                            continue        # no bound claimed
                        worst_ratio = max(worst_ratio, d / b)
                        if d > b + 1e-6:
                            violations += 1
                            print("VIOLATION flow", k, "observed", d,
                                  "bound", b)
    print(f"{path}: {header['shaping']} {header['classes']} class(es), "
          f"{len(flows)} flows, {scenarios} scenarios, "
          f"max observed/bound = {worst_ratio:.3f}, violations = {violations}")


if __name__ == "__main__":
    main()
