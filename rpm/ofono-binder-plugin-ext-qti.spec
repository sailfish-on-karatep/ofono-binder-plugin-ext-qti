Name: ofono-binder-plugin-ext-qti

# 0.0.2, not 0.0.1, so this fork outranks the prebuilt package in the
# Sailfish repos. That one is versioned 0.0.1+main.<timestamp>.<sha>, and
# RPM ranks 0.0.1+main... ABOVE a plain 0.0.1 -- so a locally built 0.0.1
# is silently ignored by mic and zypper and the image ships upstream's
# build instead. Bumping the minor version is the unambiguous fix.
Version: 0.0.2
Release: 1
Summary: QTI IRadio extension ofono binder plugin
License: GPLv2
URL: https://gitlab.com/ubports/development/core/hybris-support/ofono-binder-plugin-ext-qti
Source: %{name}-%{version}.tar.bz2
Provides: ofono-ims-support

BuildRequires: ofono-devel
BuildRequires: pkgconfig
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(libglibutil)
BuildRequires: pkgconfig(libgbinder-radio)
BuildRequires: pkgconfig(libofonobinderpluginext)

%define plugin_dir %(pkg-config ofono --variable=plugindir)
%define config_dir /etc/ofono/binder.d/

%description
QTI IRadio extension for ofono binder plugin

%prep
%setup -q -n %{name}-%{version}

%build
make %{_smp_mflags} PLUGINDIR=%{plugin_dir} KEEP_SYMBOLS=1 release

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} PLUGINDIR=%{plugin_dir} install
mkdir -p %{buildroot}%{config_dir}
install -m 644 qti.conf %{buildroot}%{config_dir}

%files
%dir %{plugin_dir}
%dir %{config_dir}
%defattr(-,root,root,-)
%config %{config_dir}/qti.conf
%{plugin_dir}/qtibinderpluginext.so
