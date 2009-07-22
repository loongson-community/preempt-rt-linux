STRIPFLAGS := --strip-all
ifdef CONFIG_32BIT
vmlinux.gz: $(TOPDIR)/vmlinux
else
vmlinux.gz: $(TOPDIR)/vmlinux.32
endif
	$(Q)echo "  GZIP   "   vmlinux
	$(Q)$(OBJCOPY) -O binary $(STRIPFLAGS) $< $(obj)/vmlinux
	$(Q)gzip -cf $(obj)/vmlinux >$(obj)/$@
