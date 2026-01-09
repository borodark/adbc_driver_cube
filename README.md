# ADBC Driver for Cube (binaries)

This repo packages the Cube ADBC driver into release artifacts for consumption
by clients like the Elixir ADBC bindings.

## Layout

- third_party/apache-arrow-adbc: vendored Arrow ADBC sources (required)
- scripts/build.sh: build the shared library
- scripts/package.sh: create a tar.gz for release
- scripts/sync_from_fork.sh: optional sync helper for refreshing ADBC sources

## Quick start (Linux amd64)

1) Ensure FlatBuffers is installed (or set `FLATBUFFERS_DIR` to its CMake config dir).

2) Build the driver:

   ./scripts/build.sh

3) Package the tarball:

   VERSION=0.1.0 TRIPLET=x86_64-linux-gnu ./scripts/package.sh

The artifact will land in dist/ as:

  adbc_driver_cube-<version>-<triplet>.tar.gz

## Release flow

- Tag a release (for example, v0.1.0) and push it.
- GitHub Actions builds the Linux amd64 artifact and attaches it to the release.

## Notes

- Only Linux amd64 is wired up for now.
- The build uses the vendored Arrow ADBC C sources.
- FlatBuffers is required; set `FLATBUFFERS_DIR` if CMake cannot find it.

## Make targets

- `make sync SRC=/path/to/adbc` (optional refresh)
- `make build`
- `make package VERSION=0.1.0 TRIPLET=x86_64-linux-gnu`
