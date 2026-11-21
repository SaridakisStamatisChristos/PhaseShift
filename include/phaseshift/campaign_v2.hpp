#pragma once
#include "phaseshift/store.hpp"
namespace ps {
// M7: one immutable family per v2 run, frozen before any holdout sample.
// Only raw positive hypotheses with independently sealed holdout evidence
// and an internally recorded valid fixed-sample sign-flip p-value can be
// family-wise accepted. Negative and incomplete tests receive p=1.
void freeze_campaign_v2(Store& store,const Json& manifest);
void record_campaign_signflip_v2(Store& store,const std::string& hypothesis_id,
 double p_value,int independent_pairs);
Json finalize_campaign_v2(Store& store,const std::string& campaign_id);
}
