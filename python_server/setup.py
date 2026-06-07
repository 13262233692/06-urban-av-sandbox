from setuptools import setup, find_packages

setup(
    name="avsandbox",
    version="1.0.0",
    description="L4 Autonomous Driving Simulation Sandbox - Ray/RLlib Control Server",
    packages=find_packages(),
    python_requires=">=3.10",
    install_requires=[
        "ray>=2.8.0",
        "gymnasium>=0.29.0",
        "numpy>=1.24.0",
        "torch>=2.1.0",
    ],
    extras_require={
        "dev": [
            "pytest>=7.0",
            "black>=23.0",
            "ruff>=0.1.0",
        ],
    },
    entry_points={
        "console_scripts": [
            "avsandbox-train=avsandbox.cli:train_entrypoint",
        ],
    },
)
