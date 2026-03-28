# SPDX-License-Identifier: Apache-2.0

Name:           opensomeip
Version:        0.0.3
Release:        1%{?dist}
Summary:        C++17 implementation of SOME/IP for automotive and embedded systems

License:        Apache-2.0
URL:            https://github.com/vtz/opensomeip
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  ninja-build

%global debug_package %{nil}

%description
OpenSOME/IP is a portable, standards-compliant C++17 implementation of
SOME/IP (Scalable service-Oriented MiddlewarE over IP). It provides
messaging, serialization, service discovery, RPC, events, transport
protocol segmentation, and E2E protection for automotive and embedded
Linux systems.

%package        devel
Summary:        Development files for %{name}
Provides:       %{name}-static = %{version}-%{release}

%description    devel
Headers and static library for developing applications with OpenSOME/IP.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake \
    -G Ninja \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DSOMEIP_DEV_TOOLS=OFF \
    -DENABLE_WERROR=OFF
%cmake_build

%install
%cmake_install

%files devel
%license LICENSE
%doc README.md CHANGELOG.md
%{_includedir}/someip/
%{_libdir}/libopensomeip.a
