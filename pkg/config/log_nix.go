// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0.
// This product includes software developed at Datadog (https://www.datadoghq.com/).
// Copyright 2016-2019 Datadog, Inc.

// +build linux freebsd netbsd openbsd solaris dragonfly darwin

package config

// GetSyslogURI returns the configured/default syslog uri
func GetSyslogURI() string {
	enabled := Datadog.GetBool("log_to_syslog")
	uri := Datadog.GetString("syslog_uri")

	if enabled {
		if uri == "" {
			uri = defaultSyslogURI
		}
	}

	return uri
}