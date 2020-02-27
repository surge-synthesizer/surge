    .section ".rodata", "a"

configurationXmlStart:
    .globl configurationXmlStart
    .incbin "../../resources/data/configuration.xml"
configurationXmlEnd:
    .global configurationXmlNullTerminator
#ifndef __arm__
    .type configurationXmlNullTerminator, @object
#endif
configurationXmlNullTerminator:
    .int 0
