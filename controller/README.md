# gdpr_controller

This is the gdpr_controller project.

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Licensing

<!--
Please go to https://choosealicense.com/licenses/ and choose a license that
fits your needs. The recommended license for a project of this type is the
GNU AGPLv3.
-->

# Hack to use cmake-init in nix
1. Get into a `nix-shell` with pip
2. Run `pip install --prefix=/path/to/install/directory/ cmake-init`
3. Run `export PYTHONPATH=$PYTHONPATH:/path/to/install/directory/lib/pythonX.Y/site-packages` (X and Y depend on your python version)
4. Find the binary in the install directory `cd /path/to/install/directory/bin`
5. Run the cmake-init binary with your desired arguments `./cmake-init {args}`