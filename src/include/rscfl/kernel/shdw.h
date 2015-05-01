#ifndef _RSCFL_SHDW_H_
#define _RSCFL_SDHW_H_

typedef int shdw_hdl;

shdw_hdl shdw_create(void);

int shdw_switch(shdw_hdl);

int shdw_switch_pages(shdw_hdl, int);

int shdw_switch(shdw_hdl);

#endif
