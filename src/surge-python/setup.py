"""
Python package details for surgepy.

Surgepy can be built and installed as a Python package by running

    $ python3 -m pip install <REPO_DIR>/src/surge-python

where <REPO_DIR> is the surge repo.

This uses scikit-build, a tool for packaging Python extensions built
with CMake. For more information see https://scikit-build.readthedocs.io
"""
from skbuild import setup
from pathlib import Path

def just_surgepy(cmake_manifest):
    return [x for x in cmake_manifest if "surgepy" in x]

def find_stubs(path: Path):
    return [str(pyi.relative_to(path)) for pyi in path.rglob("*.pyi")]


setup(
    name="surgepy",
    version="0.1.0",
    description="Python bindings for Surge XT synth",
    license="GPLv3",
    python_requires=">=3.7",
    install_requires=["numpy"],
    packages=["surgepy"],
    cmake_source_dir="../..",
    cmake_args=[
        "-DSURGE_BUILD_PYTHON_BINDINGS=TRUE",
        "-DSURGE_SKIP_JUCE_FOR_RACK=TRUE",
        "-DSURGE_SKIP_ODDSOUND_MTS=TRUE",
        "-DSURGE_SKIP_VST3=TRUE",
        "-DSURGE_SKIP_ALSA=TRUE",
        "-DSURGE_SKIP_STANDALONE=TRUE",
    ],
    cmake_process_manifest_hook=just_surgepy,
    package_data={"surgepy": [*find_stubs(path=Path("surgepy"))]},
)
