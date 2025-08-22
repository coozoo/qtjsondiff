# spectool -g -R qtjsondiff.spec
# rpmbuild -ba qtjsondiff.spec

%define name qtjsondiff
%define reponame qtjsondiff
%define version %(echo "$(curl --silent  'https://raw.githubusercontent.com/coozoo/qtjsondiff/main/main.cpp'|grep 'QString APP_VERSION'| tr -d ' '|grep -oP '(?<=constQStringAPP_VERSION=").*(?=\";)')")
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}

Summary: QT json diff UI viwer
Name: %{name}
Version: %{version}
Release: %{build_timestamp}%{?dist}
Obsoletes: QTjsonDiff
Source0: https://github.com/coozoo/qtjsondiff/archive/main.zip#/%{name}-%{version}-%{release}.tar.gz


License: MIT
Url: https://github.com/coozoo/qtjsondiff

%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires: qt6-qtbase-devel >= 6.2
BuildRequires: qt6-linguist >= 6.2
BuildRequires: desktop-file-utils
Requires:      desktop-file-utils
%endif
%if 0%{?suse_version} || 0%{?sle_version}
Group:          Development/Tools/Other
BuildRequires:  pkgconfig(Qt6Widgets)
BuildRequires:  qt6-tools-linguist
BuildRequires:  qt6-base-devel
BuildRequires:  qt6-tools-devel
BuildRequires:  update-desktop-files
BuildRequires:  zlib-devel
Requires(post): update-desktop-files
Requires(postun): update-desktop-files
%endif
%if 0%{?mageia} || 0%{?mdkversion}
BuildRequires: qtbase6-common-devel >= 6.2
BuildRequires: lib64qt6base6-devel >= 6.2
BuildRequires: lib64qt6help-devel >= 6.2
%endif


# Requires: qt6 >= 6.2

%description
Some kind of diff viewer for Json that consists of two json viewer widgets.
There is two modes to view:
json and text, search text inside json. Use different sources
of json file, url or simply copy-paste. And some more features.

%global debug_package %{nil}

%prep
%setup -q -n %{reponame}-main

%build
# don't know maybe it's stupid me but lrelease in qt looks like runs after make file generation as result automatic file list inside qmake doesn't work
# so what I need just run it twice...
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
    qmake6
    make
    qmake6
    make
%endif
%if 0%{?suse_version} || 0%{?sle_version} || 0%{?mdkversion}
    %qmake6
    %make_build
    %qmake6
    %make_build
%endif
%if 0%{?mageia}
    qmake6
    %make_build
    qmake6
    %make_build
%endif

%install
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version} || 0%{?mageia} || 0%{?mdkversion}
    make INSTALL_ROOT=%{buildroot} -j$(nproc) install
    desktop-file-install \
        --dir=%{buildroot}%{_datadir}/applications \
        %{buildroot}%{_datadir}/applications/qtjsondiff.desktop
%endif
%if 0%{?suse_version} || 0%{?sle_version}
    %qmake6_install
#    mkdir -p %{buildroot}%{_datadir}/pixmaps
#    mv %{buildroot}%{_datadir}/icons/diff.png %{buildroot}%{_datadir}/pixmaps/diff.png
    %suse_update_desktop_file -G "JSON Diff Tool" -r qtjsondiff Utility TextEditor
%endif

%post
%if 0%{?suse_version} ||  0%{?sle_version}
    %desktop_database_post
%endif

%postun
%if 0%{?suse_version} || 0%{?sle_version}
    %desktop_database_postun
%endif

%files
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version} || 0%{?mageia} || 0%{?mdkversion}
    %{_bindir}/*
    %{_datadir}/*
%endif
%if 0%{?suse_version} || 0%{?sle_version}
    %license LICENSE
    %doc README.md
    %{_bindir}/*
    %{_datadir}/*
    %{_datadir}/applications/qtjsondiff.desktop
%endif

%changelog
