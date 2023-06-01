# ESP-HAL-3RDPARTY on NuttX

This branch is a release version intended to be used by NuttX as a HAL *(Hardware Abstraction Layer*), including Wi-Fi features.

## Release nuttx-20230531

This release was based at [`sync-3-release_v5.0`](../../tree/sync-3-release_v5.0), forked on 2023-05-31 ([`833111c1e2bf5adeb5b7b118a972df2f9d2a486a`](../../tree/833111c1e2bf5adeb5b7b118a972df2f9d2a486a)), creating the branch [`test/release/v2.1`](../../tree/test/release/v2.1).

## How to Use this Release

This branch is intended to be downloaded/cloned by NuttX's build system, using the provided source to build Espressif's SoCs on NuttX.

### Organization
This branch is organized into the following directories:

#### `components`
The [`components`](./components/) folder contains the components sourced from [`sync-3-release_v5.0`](../../tree/sync-3-release_v5.0) and the additions made to enable building the sources on NuttX. Please note that these components may contain git submodules.

> **Note 1:**
> Not all sources under `components` are required to build NuttX: it's feature-dependent. Please check which sources are built on NuttX when build, for instance, ESP32-S3's Wi-Fi: [nuttx/arch/xtensa/src/esp32s3/Wireless.mk](https://github.com/apache/nuttx/blob/master/arch/xtensa/src/esp32s3/Wireless.mk).

> **Note 2:**
> Please note that, preferably, modifications on source should be made using the preprocessor macro `__NuttX__`.

#### `nuttx`

The [`nuttx`](./nuttx) directory contains two sub-directories:

##### `nuttx/include`

Includes headers that are used specifically to build NuttX that are not available in `components` (or that can't be directly added there), including API's translations, macro (re)-definitions, and other necessary files that are meant to interface with sources and header files in `components`.

##### `nuttx/patches`

Provide patches to be applied in the source code when it isn't suitable to directly change it. Specifically, patches are used to change the source code of git submodules. The Mbed TLS is a submodule in [`components/mbedtls/mbedtls`](./components/mbedtls/mbedtls) folder and directly changing its source would require keeping a separate remote for the Mbed TLS. Instead, we can provide patches to be applied before building the Mbed TLS.

> **Note 3:**
> The path under `nuttx/patches` is related to the path where the patch is meant to be applied: [`nuttx/patches/components/mbedtls/mbedtls/0001-mbedtls_add_prefix.patch`](nuttx/patches/components/mbedtls/mbedtls/0001-mbedtls_add_prefix.patch), for instance, is meant to be applied at [`components/mbedtls/mbedtls/`](components/mbedtls/mbedtls/)

### Workarounds

This section explains in-depth the workarounds for using the components on NuttX.
#### Mbed TLS Symbol Collisions

ESP32 SoC family makes use of the Mbed TLS to implement [wpa_supplicant crypto functions](components/wpa_supplicant/src/crypto). However, this same application may be present on 3rd party platforms. This is true for NuttX, for example. Thus, to provide complete userspace/kernel separation and to avoid problems regarding the Mbed TLS version, the ESP32 implementation builds a custom version of Mbed TLS. To avoid symbol collision if NuttX's Mbed TLS is used, functions and global variables with external linkage from the ESP32-custom Mbed TLS are then prefixed.

This is done through patches that apply the prefix. Please check [nuttx/patches/components/mbedtls/mbedtls](nuttx/patches/components/mbedtls/mbedtls) for checking the patches that are meant to be applied on Mbed TLS submodule that is used by the Wi-Fi driver.

## How to Update the Release Branch

From a release branch, make a rebase to get the most recent changes from a sync branch (it can be the same branch that was used for that release or another sync branch). For instance, considering that the `release/nuttx` contains the latest release for NuttX and it will be rebased into `sync-3-release_v5.0`:

```
git checkout sync-3-release_v5.0
git tag $(git branch --show-current)-nuttx-$(date +%Y%m%d)
git checkout release/nuttx
git rebase origin/sync-3-release_v5.0
```

That would throw several conflicts to be solved in the commits that are on top of the sync-branch branch. Solve them!

### Submodules
The components may contain git submodules. The sync branch provides just a reference of the hash used by these submodules, but it doesn't add them as a git submodule.

The [`sync-3-release_v5.0`](../../tree/sync-3-release_v5.0), specifically, contains the following submodules:
* components/mbedtls/mbedtls;
* components/esp_phy/lib;
* components/esp_wifi/lib;

Each one of them needs to be added as a git submodule in this release branch. Considering the `esp_phy/lib`, for instance:
```
git rm -r --cached components/esp_phy/lib
rm -rf components/esp_phy/lib
git submodule add ../../espressif/esp-phy-lib.git components/esp_phy/lib
git restore --staged components/esp_phy/lib
```

After (re)adding the git submodules, make sure they are updated to the same hash of the `components`:
```
git submodule update --init --update
``` 

### Patches

After rebasing and ensuring that the submodules are properly set and updated, one should test applying the patches in [nuttx/patches/](nuttx/patches/). If any of them fail, please fix them.

#### nuttx/patches/components/mbedtls/mbedtls

The patches under [nuttx/patches/components/mbedtls/mbedtls](nuttx/patches/components/mbedtls/mbedtls) may fail especially if the version of the Mbed TLS changes. This happens because the patches are intended to add a prefix to Mbed TLS's functions (please check [here](#mbed-tls-symbol-collisions)). If this happens, use the following method to recreate them.

##### Create ctags file

This file maps all the functions and global variables used by the Mbed TLS library. To generate it:

> **Warning**
> This requires the [`ctags`](https://github.com/universal-ctags/ctags) tool needs to be installed on the system.

```
ctags -f utils/ctags/mbedtls/tags --kinds-c=fv components/mbedtls/mbedtls/library/*.c
```

> **Note 4:**
> This file only needs to be updated when Mbed TLS submodule is updated. This file is versioned in [utils/ctags/mbedtls/tags](utils/ctags/mbedtls/tags).

##### Generate Prefix Patch for Mbed TLS

Use the [prefixer](utils/prefixer.sh) script to add the `esp_` prefix to Mbed TLS-related functions and variables.

```
cd utils/
git -C ../components/mbedtls/mbedtls reset --hard
./prefixer.sh ctags/mbedtls/tags ../components/mbedtls/mbedtls
mkdir -p ../nuttx/patches/components/mbedtls/mbedtls
git -C ../components/mbedtls/mbedtls diff --full-index --binary > ../nuttx/patches/components/mbedtls/mbedtls/0001-mbedtls_add_prefix.patch
git -C ../esp-idf/components/mbedtls/mbedtls reset --hard
```

> **Note 5:**
> Please note that `ctags` only uses the first branch on a `#if` (or `#ifdef`) [preprocessor conditional](https://docs.ctags.io/en/latest/man/ctags.1.html#notes-for-c-c-parser). That being said, it may need to be necessary to add the `esp_` prefix manually for some functions. Please check [`0002-mbedtls_add_prefix.patch`](nuttx/patches/components/mbedtls/mbedtls/0002-mbedtls_add_prefix.patch) for an example of a patch that added the `esp_` prefix manually. The same is valid for [macro-defined code](nuttx/patches/components/mbedtls/mbedtls/0003-mbedtls_add_prefix_to_macro.patch).

### Tagging the Release
After the code was 1) rebased, 2) submodules configured, and 3) patches updated. Please tag the version before pushing the branche release branch upstream:

```
# Create an annotated tag for identifying the release
git tag -a nuttx-$(date +%Y%m%d) -m "NuttX Release $(date +%Y%m%d)"

# Push the branches and tags to upstream
git push -f esp release/nuttx
git push --tags
```
