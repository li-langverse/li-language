//! Lennard-Jones MD — SoA stack arrays, naive O(N^2) (fastest at N=256).

const N: usize = 256;
const STEPS: usize = 10_000;
const DT: f64 = 0.004;
const RC: f64 = 2.5;
const BOX: f64 = 10.0;
const SEED: u64 = 7;
const TRACE_INTERVAL: usize = 25;

struct Rng {
    state: u64,
}

impl Rng {
    fn new(seed: u64) -> Self {
        Self { state: seed }
    }

    fn next_f64(&mut self) -> f64 {
        self.state = self.state.wrapping_mul(6364136223846793005).wrapping_add(1);
        ((self.state >> 11) as f64) / ((1u64 << 53) as f64)
    }
}

struct State {
    px: [f64; N],
    py: [f64; N],
    pz: [f64; N],
    vx: [f64; N],
    vy: [f64; N],
    vz: [f64; N],
    fx: [f64; N],
    fy: [f64; N],
    fz: [f64; N],
}

fn init_lattice(s: &mut State, rng: &mut Rng) {
    let cells = (N as f64).cbrt().ceil() as usize;
    let spacing = BOX / cells as f64;
    let mut idx = 0usize;
    for ix in 0..cells {
        for iy in 0..cells {
            for iz in 0..cells {
                if idx >= N {
                    break;
                }
                s.px[idx] = (ix as f64 + 0.5) * spacing;
                s.py[idx] = (iy as f64 + 0.5) * spacing;
                s.pz[idx] = (iz as f64 + 0.5) * spacing;
                s.vx[idx] = 0.01 * (rng.next_f64() - 0.5);
                s.vy[idx] = 0.01 * (rng.next_f64() - 0.5);
                s.vz[idx] = 0.01 * (rng.next_f64() - 0.5);
                idx += 1;
            }
        }
    }
}

fn mic(d: f64) -> f64 {
    let half = 0.5 * BOX;
    if d > half {
        d - BOX
    } else if d < -half {
        d + BOX
    } else {
        d
    }
}

fn wrap_pos(x: f64) -> f64 {
    let mut v = x % BOX;
    if v < 0.0 {
        v += BOX;
    }
    v
}

fn kinetic(s: &State) -> f64 {
    let mut ke = 0.0;
    for i in 0..N {
        ke += 0.5 * (s.vx[i] * s.vx[i] + s.vy[i] * s.vy[i] + s.vz[i] * s.vz[i]);
    }
    ke
}

fn potential(s: &State) -> f64 {
    let rc2 = RC * RC;
    let mut pe = 0.0;
    for i in 0..N {
        for j in (i + 1)..N {
            let dx = mic(s.px[j] - s.px[i]);
            let dy = mic(s.py[j] - s.py[i]);
            let dz = mic(s.pz[j] - s.pz[i]);
            let r2 = dx * dx + dy * dy + dz * dz;
            if r2 >= rc2 || r2 < 1e-12 {
                continue;
            }
            let inv_r2 = 1.0 / r2;
            let inv_r6 = inv_r2 * inv_r2 * inv_r2;
            let inv_r12 = inv_r6 * inv_r6;
            pe += 4.0 * (inv_r12 - inv_r6);
        }
    }
    pe
}

fn compute_forces(s: &mut State) {
    let rc2 = RC * RC;
    s.fx = [0.0; N];
    s.fy = [0.0; N];
    s.fz = [0.0; N];
    for i in 0..N {
        for j in (i + 1)..N {
            let dx = mic(s.px[j] - s.px[i]);
            let dy = mic(s.py[j] - s.py[i]);
            let dz = mic(s.pz[j] - s.pz[i]);
            let r2 = dx * dx + dy * dy + dz * dz;
            if r2 >= rc2 || r2 < 1e-12 {
                continue;
            }
            let inv_r2 = 1.0 / r2;
            let inv_r6 = inv_r2 * inv_r2 * inv_r2;
            let inv_r12 = inv_r6 * inv_r6;
            let f_scalar = 48.0 * inv_r12 - 24.0 * inv_r6;
            let fx = f_scalar * dx;
            let fy = f_scalar * dy;
            let fz = f_scalar * dz;
            s.fx[i] -= fx;
            s.fy[i] -= fy;
            s.fz[i] -= fz;
            s.fx[j] += fx;
            s.fy[j] += fy;
            s.fz[j] += fz;
        }
    }
}

fn step(s: &mut State) {
    for i in 0..N {
        s.vx[i] += 0.5 * DT * s.fx[i];
        s.vy[i] += 0.5 * DT * s.fy[i];
        s.vz[i] += 0.5 * DT * s.fz[i];
    }
    for i in 0..N {
        s.px[i] = wrap_pos(s.px[i] + DT * s.vx[i]);
        s.py[i] = wrap_pos(s.py[i] + DT * s.vy[i]);
        s.pz[i] = wrap_pos(s.pz[i] + DT * s.vz[i]);
    }
    compute_forces(s);
    for i in 0..N {
        s.vx[i] += 0.5 * DT * s.fx[i];
        s.vy[i] += 0.5 * DT * s.fy[i];
        s.vz[i] += 0.5 * DT * s.fz[i];
    }
}

fn run(trace_path: Option<&std::path::Path>) -> f64 {
    let mut rng = Rng::new(SEED);
    let mut s = State {
        px: [0.0; N],
        py: [0.0; N],
        pz: [0.0; N],
        vx: [0.0; N],
        vy: [0.0; N],
        vz: [0.0; N],
        fx: [0.0; N],
        fy: [0.0; N],
        fz: [0.0; N],
    };
    init_lattice(&mut s, &mut rng);
    compute_forces(&mut s);
    let mut trace = trace_path.map(std::fs::File::create).transpose().ok().flatten();
    if let Some(ref mut f) = trace {
        use std::io::Write;
        writeln!(f, "step,pe,ke,etotal").ok();
        writeln!(f, "0,{},{},{}", potential(&s), kinetic(&s), potential(&s) + kinetic(&s)).ok();
    }
    let e0 = potential(&s) + kinetic(&s);
    for step_i in 1..=STEPS {
        step(&mut s);
        if let Some(ref mut f) = trace {
            if step_i % TRACE_INTERVAL == 0 || step_i == STEPS {
                use std::io::Write;
                let pe = potential(&s);
                let ke = kinetic(&s);
                writeln!(f, "{step_i},{pe},{ke},{}", pe + ke).ok();
            }
        }
    }
    let e1 = potential(&s) + kinetic(&s);
    let denom = e0.abs().max(e1.abs()).max(1e-12);
    (e1 - e0).abs() / denom
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let trace_path = args
        .windows(2)
        .find(|w| w[0] == "--trace")
        .map(|w| std::path::Path::new(&w[1]));
    let drift = run(trace_path);
    if args.iter().any(|a| a == "--verify") {
        println!("{:.8e}", drift);
    }
}
