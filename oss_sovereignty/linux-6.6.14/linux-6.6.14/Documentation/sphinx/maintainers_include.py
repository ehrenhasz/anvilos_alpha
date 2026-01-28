u"""
    maintainers-include
    ~~~~~~~~~~~~~~~~~~~
    Implementation of the ``maintainers-include`` reST-directive.
    :copyright:  Copyright (C) 2019  Kees Cook <keescook@chromium.org>
    :license:    GPL Version 2, June 1991 see linux/COPYING for details.
    The ``maintainers-include`` reST-directive performs extensive parsing
    specific to the Linux kernel's standard "MAINTAINERS" file, in an
    effort to avoid needing to heavily mark up the original plain text.
"""
import sys
import re
import os.path
from docutils import statemachine
from docutils.utils.error_reporting import ErrorString
from docutils.parsers.rst import Directive
from docutils.parsers.rst.directives.misc import Include
__version__  = '1.0'
def setup(app):
    app.add_directive("maintainers-include", MaintainersInclude)
    return dict(
        version = __version__,
        parallel_read_safe = True,
        parallel_write_safe = True
    )
class MaintainersInclude(Include):
    u"""MaintainersInclude (``maintainers-include``) directive"""
    required_arguments = 0
    def parse_maintainers(self, path):
        """Parse all the MAINTAINERS lines into ReST for human-readability"""
        result = list()
        result.append(".. _maintainers:")
        result.append("")
        descriptions = False
        maintainers = False
        subsystems = False
        field_letter = None
        fields = dict()
        prev = None
        field_prev = ""
        field_content = ""
        for line in open(path):
            if descriptions and line.startswith('Maintainers'):
                descriptions = False
                result.append("")
            if maintainers and not subsystems:
                if re.search('^[A-Z0-9]', line):
                    subsystems = True
            line = line.rstrip()
            pat = '(Documentation/([^\s\?\*]*)\.rst)'
            m = re.search(pat, line)
            if m:
                line = re.sub(pat, ':doc:`%s <../%s>`' % (m.group(2), m.group(2)), line)
            output = None
            if descriptions:
                output = "| %s" % (line.replace("\\", "\\\\"))
                m = re.search("\s(\S):\s", line)
                if m:
                    field_letter = m.group(1)
                if field_letter and not field_letter in fields:
                    m = re.search("\*([^\*]+)\*", line)
                    if m:
                        fields[field_letter] = m.group(1)
            elif subsystems:
                if len(line) == 0:
                    continue
                if line[1] != ':':
                    output = field_content + "\n\n"
                    field_content = ""
                    heading = re.sub("\s+", " ", line)
                    output = output + "%s\n%s" % (heading, "~" * len(heading))
                    field_prev = ""
                else:
                    field, details = line.split(':', 1)
                    details = details.strip()
                    if field in ['F', 'N', 'X', 'K']:
                        if not ':doc:' in details:
                            details = '``%s``' % (details)
                    if field == field_prev and field_prev in ['M', 'R', 'L']:
                        field_content = field_content + ","
                    if field != field_prev:
                        output = field_content + "\n"
                        field_content = ":%s:" % (fields.get(field, field))
                    field_content = field_content + "\n\t%s" % (details)
                    field_prev = field
            else:
                output = line
            if output != None:
                for separated in output.split('\n'):
                    result.append(separated)
            if line.startswith('----------'):
                if prev.startswith('Descriptions'):
                    descriptions = True
                if prev.startswith('Maintainers'):
                    maintainers = True
            prev = line
        if field_content != "":
            for separated in field_content.split('\n'):
                result.append(separated)
        output = "\n".join(result)
        self.state_machine.insert_input(
          statemachine.string2lines(output), path)
    def run(self):
        """Include the MAINTAINERS file as part of this reST file."""
        if not self.state.document.settings.file_insertion_enabled:
            raise self.warning('"%s" directive disabled.' % self.name)
        path = self.state_machine.document.attributes['source']
        path = os.path.realpath(path)
        tail = path
        while tail != "Documentation" and tail != "":
            (path, tail) = os.path.split(path)
        path = os.path.join(path, "MAINTAINERS")
        try:
            self.state.document.settings.record_dependencies.add(path)
            lines = self.parse_maintainers(path)
        except IOError as error:
            raise self.severe('Problems with "%s" directive path:\n%s.' %
                      (self.name, ErrorString(error)))
        return []
