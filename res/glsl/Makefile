GLSLC := glslc

VERTSOURCE := $(wildcard *.vert)
GEOMSOURCE := $(wildcard *.geom)
FRAGSOURCE := $(wildcard *.frag)
COMPSOURCE := $(wildcard *.comp)

SPVDIR := ../spv/

VERTSPV := $(patsubst %,$(SPVDIR)%,$(VERTSOURCE:.vert=.vert.spv))
GEOMSPV := $(patsubst %,$(SPVDIR)%,$(GEOMSOURCE:.geom=.geom.spv))
FRAGSPV := $(patsubst %,$(SPVDIR)%,$(FRAGSOURCE:.frag=.frag.spv))
COMPSPV := $(patsubst %,$(SPVDIR)%,$(COMPSOURCE:.comp=.comp.spv))

all: $(VERTSPV) $(GEOMSPV) $(FRAGSPV) $(COMPSPV)
	
$(SPVDIR)%.vert.spv: %.vert
	$(GLSLC) -c --target-env=vulkan -fshader-stage=vert -o $@ $^

$(SPVDIR)%.geom.spv: %.geom
	$(GLSLC) -c --target-env=vulkan -fshader-stage=geom -o $@ $^

$(SPVDIR)%.frag.spv: %.frag
	$(GLSLC) -c --target-env=vulkan -fshader-stage=frag -o $@ $^

$(SPVDIR)%.comp.spv: %.comp
	$(GLSLC) -c --target-env=vulkan -fshader-stage=comp -o $@ $^

clean:
	rm ../spv/*spv

