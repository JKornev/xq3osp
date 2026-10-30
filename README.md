# xq3osp

Quake 3 OSP mod (recovered, byte-identical)

This is a reverse-engineered source code of **OSP Tourney DM/CA/CTF 1.03a** for Quake III
Arena, the mod by Jason "Rhea" Keimig and the OrangeSmoothie team whose source was never
released. All three game modules were restored from the shipped QVM bytecode. Compiled with
id Software's original 1.32b `q3lcc` and `q3asm`, the sources produce files that are
**byte-identical** to the QVMs OSP shipped in 2002 and 2003:

| Module | Shipped in | SHA-256 of the QVM |
|---|---|---|
| `qagame` (server) | `zz-osp-server3a.pk3`, 4 Mar 2003 | `7ce6a6ac1fcb444206461e19129d4f5cd1c4ba79a3cbf881653e6450b8277160` |
| `cgame` (client) | `zz-osp-pak3.pk3`, 28 Feb 2003 | `48e17665b0b274baa95670364364be373fbbda4aea6e74c1fb587aaa5feb33b3` |
| `ui` (menus) | `zz-osp-pak1.pk3`, 13 Jan 2002 | `36133dea02f56992a8683f852893a414e330cc87063895671ee268b51891147c` |

## Build

Only Docker is needed on the host: Docker Desktop on Windows, Docker Engine on Linux.
Run everything from the repository root.

Build the toolchain image once (id's 1.32b `q3lcc` and `q3asm`, built from `src/tools`):

```sh
docker build -f docker/Dockerfile -t xq3osp-build .
```

Then build a module (`MODULE` is `qagame`, `cgame` or `ui`). The same command works in
PowerShell and in Linux shells:

```sh
docker run --rm -v "${PWD}:/work" -w /work xq3osp-build make -C build MODULE=qagame all
```

The result is `build/vm/<module>.qvm`. To use it, pack it into a pk3 as `vm/<module>.qvm`
in the mod folder; engines read pk3 contents before loose files.

| Target | What it does |
|---|---|
| `all` (default) | compile and link `build/vm/<module>.qvm`; incremental, only changed files are rebuilt |
| `info` | build and print the QVM's size and SHA-256 |
| `verify` | clean build, then compare the SHA-256 with the shipped QVM's hash (`EXPECTED_SHA256` in `build/<module>.mk`); `verify: OK` or `verify: FAIL` with non-zero exit |
| `clean` / `clean-all` | remove one module's outputs / all outputs |

## Credits

The restoration would not have been possible without these projects:

- [BigIndian](https://github.com/snems/BigIndian)
- [OSP2](https://github.com/snems/OSP2)
- [q3vm](https://github.com/brugal/q3vm)
- [baseq3a](https://github.com/ec-/baseq3a)
- [Quake III Arena 1.32b source](https://github.com/id-Software/Quake-III-Arena)
- dlmalloc by Doug Lea
- OSP Tourney by the OrangeSmoothie team

## License

Fuck license. All rights reversed ®
