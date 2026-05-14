using Printf

# Lennard-Jones MD reference — params match params.toml
const N = 256
const STEPS = 10_000
const DT = 0.004
const RC = 2.5
const BOX = 10.0
const SEED = UInt64(7)
const TRACE_INTERVAL = 25

mutable struct Rng
    state::UInt64
end

function next_f64!(rng::Rng)::Float64
    rng.state = rng.state * 6364136223846793005 + 1
    Float64(rng.state >> 11) / Float64(1 << 53)
end

function init_lattice!(pos::Matrix{Float64}, vel::Matrix{Float64}, rng::Rng)
    cells = ceil(Int, cbrt(N))
    spacing = BOX / cells
    idx = 1
    @inbounds for ix in 1:cells, iy in 1:cells, iz in 1:cells
        idx > N && break
        pos[1, idx] = (ix - 0.5) * spacing
        pos[2, idx] = (iy - 0.5) * spacing
        pos[3, idx] = (iz - 0.5) * spacing
        vel[1, idx] = 0.01 * (next_f64!(rng) - 0.5)
        vel[2, idx] = 0.01 * (next_f64!(rng) - 0.5)
        vel[3, idx] = 0.01 * (next_f64!(rng) - 0.5)
        idx += 1
    end
end

@inline function mic(d::Float64)::Float64
    half = 0.5 * BOX
    if d > half
        return d - BOX
    elseif d < -half
        return d + BOX
    end
    d
end

function compute_forces!(pos::Matrix{Float64}, forces::Matrix{Float64})
    fill!(forces, 0.0)
    rc2 = RC * RC
    @inbounds for i in 1:N
        for j in (i + 1):N
            dx = mic(pos[1, j] - pos[1, i])
            dy = mic(pos[2, j] - pos[2, i])
            dz = mic(pos[3, j] - pos[3, i])
            r2 = dx * dx + dy * dy + dz * dz
            if r2 >= rc2 || r2 < 1e-12
                continue
            end
            inv_r2 = 1.0 / r2
            inv_r6 = inv_r2 * inv_r2 * inv_r2
            inv_r12 = inv_r6 * inv_r6
            f_scalar = 48.0 * inv_r12 - 24.0 * inv_r6
            fx = f_scalar * dx
            fy = f_scalar * dy
            fz = f_scalar * dz
            forces[1, i] -= fx
            forces[2, i] -= fy
            forces[3, i] -= fz
            forces[1, j] += fx
            forces[2, j] += fy
            forces[3, j] += fz
        end
    end
end

function kinetic_energy(vel::Matrix{Float64})::Float64
    ke = 0.0
    @inbounds for i in 1:N
        ke += 0.5 * (vel[1, i]^2 + vel[2, i]^2 + vel[3, i]^2)
    end
    ke
end

function potential_energy(pos::Matrix{Float64})::Float64
    rc2 = RC * RC
    pe = 0.0
    @inbounds for i in 1:N
        for j in (i + 1):N
            dx = mic(pos[1, j] - pos[1, i])
            dy = mic(pos[2, j] - pos[2, i])
            dz = mic(pos[3, j] - pos[3, i])
            r2 = dx * dx + dy * dy + dz * dz
            if r2 >= rc2 || r2 < 1e-12
                continue
            end
            inv_r2 = 1.0 / r2
            inv_r6 = inv_r2 * inv_r2 * inv_r2
            inv_r12 = inv_r6 * inv_r6
            pe += 4.0 * (inv_r12 - inv_r6)
        end
    end
    pe
end

function record_energy(io::IO, step::Int, pos::Matrix{Float64}, vel::Matrix{Float64})
    pe = potential_energy(pos)
    ke = kinetic_energy(vel)
    @printf(io, "%d,%.12e,%.12e,%.12e\n", step, pe, ke, pe + ke)
end

function run_md(trace_path::Union{Nothing,String} = nothing)::Float64
    rng = Rng(SEED)
    pos = Matrix{Float64}(undef, 3, N)
    vel = Matrix{Float64}(undef, 3, N)
    forces = zeros(Float64, 3, N)
    trace_io = trace_path === nothing ? nothing : open(trace_path, "w")

    init_lattice!(pos, vel, rng)
    compute_forces!(pos, forces)
    if trace_io !== nothing
        println(trace_io, "step,pe,ke,etotal")
        record_energy(trace_io, 0, pos, vel)
    end
    e0 = potential_energy(pos) + kinetic_energy(vel)

    for step in 1:STEPS
        @inbounds for i in 1:N
            vel[1, i] += 0.5 * DT * forces[1, i]
            vel[2, i] += 0.5 * DT * forces[2, i]
            vel[3, i] += 0.5 * DT * forces[3, i]
        end
        @inbounds for i in 1:N
            pos[1, i] = mod(pos[1, i] + DT * vel[1, i], BOX)
            pos[2, i] = mod(pos[2, i] + DT * vel[2, i], BOX)
            pos[3, i] = mod(pos[3, i] + DT * vel[3, i], BOX)
        end
        compute_forces!(pos, forces)
        @inbounds for i in 1:N
            vel[1, i] += 0.5 * DT * forces[1, i]
            vel[2, i] += 0.5 * DT * forces[2, i]
            vel[3, i] += 0.5 * DT * forces[3, i]
        end
        if trace_io !== nothing && (step % TRACE_INTERVAL == 0 || step == STEPS)
            record_energy(trace_io, step, pos, vel)
        end
    end

    if trace_io !== nothing
        close(trace_io)
    end

    e1 = potential_energy(pos) + kinetic_energy(vel)
    denom = max(abs(e0), abs(e1), 1e-12)
    abs(e1 - e0) / denom
end

function main()
    trace_path = nothing
    for (i, arg) in enumerate(ARGS)
        if arg == "--trace" && i < length(ARGS)
            trace_path = ARGS[i + 1]
        end
    end
    verify = "--verify" in ARGS
    drift = run_md(trace_path)
    if verify
        @printf("%.8e\n", drift)
    end
end

main()
