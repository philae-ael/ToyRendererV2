import matplotlib.pyplot as plt
import numpy as np
import argparse


def poisson():
    pass


def uniform(N):
    rng = np.random.default_rng()
    theta = rng.uniform(0, 2 * np.pi, N)
    phi = rng.uniform(0, np.pi / 2, N)
    r = rng.uniform(0, 1, N)
    r = r * r

    t = np.cos(phi)

    x = r * t * np.cos(theta)
    y = r * np.sin(phi)
    z = r * t * np.sin(theta)
    return x, y, z


def export(x, y, z):
    N = x.size
    print(f"const vec4 n[{N}] = vec4[](")
    print(",\n".join(f"    vec4({a}, {b}, {c}, 0)" for a, b, c in zip(x, y, z)))
    print(");")


def main(algo, output, N):
    match algo:
        case "uniform":
            x, y, z = uniform(N)
        # case "poisson":
        #     theta, phi = uniform()
        case _:
            raise Exception("incorrect algorithm")

    match output:
        case "plot":
            _, ax = plt.subplots(subplot_kw={"projection": "3d"})
            origins = np.zeros_like(x)
            ax.quiver(origins, origins, origins, x, y, z, length=1)
            plt.show()
        case "plot2d":
            v = np.transpose([x, y, z])
            plt.hist(np.linalg.norm(v, axis=1), density=True, bins=500)
            plt.show()
        case "c":
            export(x, y, z)
        case _:
            raise Exception("incorrect algorithm")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-o", "--output", choices=["c", "plot", "plot2d"], default="plot"
    )
    parser.add_argument("-N", "--samples", default=64, type=int)
    parser.add_argument("algorithm", nargs="?", choices=["uniform"], default="uniform")

    args = parser.parse_args()
    main(args.algorithm, args.output, args.samples)
