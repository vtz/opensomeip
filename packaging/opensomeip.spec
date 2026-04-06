# SPDX-License-Identifier: Apache-2.0

Name:           opensomeip
Version:        0.0.5
Release:        1%{?dist}
Summary:        C++17 implementation of SOME/IP for automotive and embedded systems

License:        Apache-2.0
URL:            https://github.com/vtz/opensomeip
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  ninja-build

%description
OpenSOME/IP is a portable, standards-compliant C++17 implementation of
SOME/IP (Scalable service-Oriented MiddlewarE over IP). It provides
messaging, serialization, service discovery, RPC, events, transport
protocol segmentation, and E2E protection for automotive and embedded
Linux systems.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}
Provides:       %{name}-static = %{version}-%{release}

%description    devel
Headers, static library, and shared-library symlink for developing
applications with OpenSOME/IP.

%prep
%autosetup -n %{name}-%{version}

%build
# Build shared library (installed via %cmake_install)
%cmake \
    -G Ninja \
    -DBUILD_SHARED_LIBS:BOOL=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DSOMEIP_DEV_TOOLS=OFF \
    -DENABLE_WERROR=OFF
%cmake_build

# Build static library in a separate directory
cmake -B build-static -G Ninja \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DBUILD_SHARED_LIBS:BOOL=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DSOMEIP_DEV_TOOLS=OFF \
    -DENABLE_WERROR=OFF
cmake --build build-static %{?_smp_mflags}

%install
%cmake_install
install -Dpm 0644 build-static/lib/libopensomeip.a %{buildroot}%{_libdir}/libopensomeip.a

%files
%license LICENSE
%{_libdir}/libopensomeip.so.*

%files devel
%doc README.md CHANGELOG.md
%{_includedir}/someip/
%{_libdir}/libopensomeip.a
%{_libdir}/libopensomeip.so
