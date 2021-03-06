// # 2018 Travis Moore, Kedar Iyer, Sam Kazemian
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMmdhhydNMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMNdy    hMMMMMMNdhhmMMMddMMMMMMMMMMMMM
// # MMMMMMMMMMMmh      hMMMMMMh     yMMM  hNMMMMMMMMMM
// # MMMMMMMMMNy       yMMMMMMh       MMMh   hNMMMMMMMM
// # MMMMMMMMd         dMMMMMM       hMMMh     NMMMMMMM
// # MMMMMMMd          dMMMMMN      hMMMm       mMMMMMM
// # MMMMMMm           yMMMMMM      hmmh         NMMMMM
// # MMMMMMy            hMMMMMm                  hMMMMM
// # MMMMMN             hNMMMMMmy                 MMMMM
// # MMMMMm          ymMMMMMMMMmd                 MMMMM
// # MMMMMm         dMMMMMMMMd                    MMMMM
// # MMMMMMy       mMMMMMMMm                     hMMMMM
// # MMMMMMm      dMMMMMMMm                      NMMMMM
// # MMMMMMMd     NMMMMMMM                      mMMMMMM
// # MMMMMMMMd    NMMMMMMN                     mMMMMMMM
// # MMMMMMMMMNy  mMMMMMMM                   hNMMMMMMMM
// # MMMMMMMMMMMmyyNMMMMMMm         hmh    hNMMMMMMMMMM
// # MMMMMMMMMMMMMNmNMMMMMMMNmdddmNNd   ydNMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMNdhyhdmMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMNNMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
// # MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM

#include <eosiolib/eosio.hpp>
#include <ctime>

using namespace eosio;

// const uint32_t DEFAULT_VOTING_TIME = 86400; // 1 day
// const uint32_t DEFAULT_VOTING_TIME = 60; // 1 minute
const uint32_t DEFAULT_VOTING_TIME = 300; // 5 minutes
// const uint64_t STAKING_DURATION = 21 * 86400; // 21 days
const uint32_t STAKING_DURATION = 30; // 30 sec, for testing
const uint32_t REWARD_INTERVAL = 1800; // 30 min
const uint64_t EDIT_PROPOSE_BRAINPOWER = 10;
const uint64_t IQ_TO_BRAINPOWER_RATIO = 1;
const uint64_t IQ_PRECISION_MULTIPLIER = 1000;
const uint64_t GENESIS_TOKEN_SUPPLY = 10000000000;
const float ANNUAL_MINT_RATE = .025f;
const double PERIOD_REWARD_AMOUNT = 234.8993; // calculated from formula. Should be slightly less than ANNUAL_MINT_RATE * 10,000,000,000
const float EDITOR_REWARD_RATIO = 0.8f;
const float CURATION_REWARD_RATIO = 0.2f;
const uint64_t PERIOD_CURATION_REWARD = PERIOD_REWARD_AMOUNT * CURATION_REWARD_RATIO * IQ_PRECISION_MULTIPLIER;
const uint64_t PERIOD_EDITOR_REWARD = PERIOD_REWARD_AMOUNT * EDITOR_REWARD_RATIO * IQ_PRECISION_MULTIPLIER;
const float TIER_ONE_THRESHOLD = .5f;
const float TIER_THREE_THRESHOLD = .75f;
const account_name ARTICLE_CONTRACT_ACCTNAME = N(eparticlectr);
const account_name TOKEN_CONTRACT_ACCTNAME = N(epiqtokenctr);

class eparticlectr : public eosio::contract {

private:
    symbol_type IQSYMBOL = eosio::symbol_type(eosio::string_to_symbol(4, "IQ"));

    // returning array types from a DB type struct throws
    // using vectors for now, will try to use arrays later
    //using ipfshash_t = unsigned char[34];
    using byte = unsigned char;
    using ipfshash_t = std::string;
    using longdub_t = long double;
    enum ProposalStatus { pending, accepted, rejected };

public:
    static eosio::key256 ipfs_to_key256(const ipfshash_t& input); // used for the secondary index since std::string indexes are not available
    static uint64_t ipfs_to_uint64_trunc(const ipfshash_t& input); // used for the secondary index since std::string indexes are not available
    static uint64_t string_to_nameMODIFIED( const char* str );

    struct account {
       asset    balance;
       uint64_t primary_key()const { return balance.symbol.name(); }
    };

    typedef eosio::multi_index<N(accounts), account> accounts;

private:

    // ==================================================
    // ==================================================
    // ==================================================
    // DATABASE SCHEMAS

    // Wiki articles
    // @abi table
    struct wiki {
          uint64_t id;
          ipfshash_t hash; // IPFS hash of the current consensus article version
          ipfshash_t parent_hash; // IPFS hash of the parent article versiond

          uint64_t primary_key () const { return id; }
          key256 get_hash_key256 () const { return eparticlectr::ipfs_to_key256(hash); }
          key256 get_parent_hash_key256 () const { return eparticlectr::ipfs_to_key256(parent_hash); }
    };

    // Edit Proposals
    // @abi table
    struct editproposal {
          uint64_t id;
          ipfshash_t proposed_article_hash; // IPFS hash of the proposed new version
          ipfshash_t old_article_hash; // IPFS hash of the old version
          ipfshash_t grandparent_hash; // IPFS hash of the grandparent hash
          account_name proposer; // account name of the proposer
          account_name proposer_64t; // account name of the proposer in integer form
          uint64_t threshold; // abs((yes votes) - (no votes))
          uint32_t tier;
          uint32_t starttime; // epoch time of the proposal
          uint32_t endtime;
          uint32_t finalized_time; // when finalize() was called
          uint32_t status;

          uint64_t primary_key () const { return id; }
          key256 get_hash_key256 () const { return eparticlectr::ipfs_to_key256(proposed_article_hash); }
          uint64_t get_finalize_period()const { return (finalized_time / REWARD_INTERVAL); } // truncate to the nearest period
          account_name get_proposer () const { return proposer; }
          uint64_t get_proposer64t () const { return proposer_64t; }

    };

    // Voting tally
    // @abi table
    struct vote {
          uint64_t id;
          uint64_t proposal_id;
          ipfshash_t proposed_article_hash; // IPFS hash of the proposed new version
          bool approve;
          bool is_editor;
          uint64_t amount;
          account_name voter; // account name of the voter
          account_name voter_64t; // account name of the voter in integer form
          uint32_t timestamp; // epoch time of the vote

          uint64_t primary_key()const { return id; }
          key256 get_hash_key256 () const { return eparticlectr::ipfs_to_key256(proposed_article_hash); }
          uint64_t get_proposal_id()const { return id; }
          uint64_t get_voter64t()const { return voter_64t; }
    };

    // Internal struct for stakes within brainpower
    // @abi table
    struct stake {
        uint64_t id;
        account_name user;
        account_name user_64t; // account name of the user in integer form
        uint64_t amount;
        uint32_t timestamp;
        uint32_t completion_time;
        bool autorenew = 0;

        auto primary_key()const { return id; }
        account_name get_user()const { return user; }
        uint64_t get_user64t()const { return user_64t; }
    };

    // Internal struct for history of success rewards and reject slashes
    // @abi table
    struct rewardhistory {
        uint64_t id;
        account_name user;
        account_name user_64t; // account name of the user in integer form
        uint64_t amount; // slash or reward amount
        uint64_t proposal_id; // id of the proposal that this person voted on
        uint32_t proposal_finalize_time; // when finalize() was called
        uint32_t tier;
        bool proposalresult = 0;
        bool is_editor = 0;
        bool rewardtype = 0; // 0 for reject/slash, 1 for reward/success
        bool disbursed = 0; // slashes will be done immediately at finalize(). Rewards will be done at 24hr periods

        auto primary_key()const { return id; }
        account_name get_user()const { return user; }
        uint64_t get_proposal()const { return proposal_id; }
        uint64_t get_finalize_period()const { return (proposal_finalize_time / REWARD_INTERVAL); } // truncate to the nearest period
        uint64_t get_user64t()const { return user_64t; }
    };

    // test struct
    // @abi table
    struct teststruct {
        uint64_t id;

        uint64_t primary_key()const { return id; }
    };

    // Brainpower balances
    // @abi table
    struct brainpower {
        account_name user;
        uint64_t user_64t;
        uint64_t power = 0; // TODO: need to fix this later

        uint64_t primary_key()const { return user_64t; }
        account_name get_user()const { return user; }
        uint64_t get_power()const { return power; }

        // subtraction with underflow check
        uint64_t sub (uint64_t value) {
            eosio_assert(value != 0, "Please supply a nonzero value of brainpower to subtract");
            eosio_assert(value <= power, "Underflow during subtraction");
            power -= value;
            return power;
        }

        // addition with overflow check
        uint64_t add (uint64_t value) {
            eosio_assert(value != 0, "Please supply a nonzero value of brainpower to add");
            eosio_assert(value + power >= value && value + power > power, "Overflow during addition");
            power += value;
            print( "Added brainpower, ", name{power} );
            return power;
        }
    };

    //  ==================================================
    //  ==================================================
    //  ==================================================
    // DATABASE TABLES
    // GUIDE: https://github.com/EOSIO/eos/wiki/Persistence-API

    // wikis table
    // indexed by wiki hash
    // indexed by parent hash
    // @abi table
    typedef eosio::multi_index<N(wikistbl), wiki,
        indexed_by< N(byhash), const_mem_fun< wiki, eosio::key256, &wiki::get_hash_key256 >>,
        indexed_by< N(byoldhash), const_mem_fun< wiki, eosio::key256, &wiki::get_parent_hash_key256 >>
    > wikistbl; // EOS table for the articles


    // edit proposals table
    // 12-char limit on table names, so proposals used instead of editproposals
    // indexed by hash
    // @abi table
    typedef eosio::multi_index<N(propstbl), editproposal,
        indexed_by< N(byhash), const_mem_fun< editproposal, eosio::key256, &editproposal::get_hash_key256 >>,
        indexed_by< N(byproposer64t), const_mem_fun< editproposal, uint64_t, &editproposal::get_proposer64t >>,
        indexed_by< N(byfinalper), const_mem_fun< editproposal, uint64_t, &editproposal::get_finalize_period >>
    > propstbl; // EOS table for the edit proposals

    // votes table
    // indexed by proposal
    // @abi table
    typedef eosio::multi_index<N(votestbl), vote,
        indexed_by< N(byhash), const_mem_fun< vote, eosio::key256, &vote::get_hash_key256 >>,
        indexed_by< N(byproposal), const_mem_fun< vote, uint64_t, &vote::get_proposal_id >>,
        indexed_by< N(byvoter64t), const_mem_fun< vote, uint64_t, &vote::get_voter64t >>
    > votestbl; // EOS table for the votes

    // brainpower table
    // @abi table
    typedef eosio::multi_index<N(brainpwrtbl), brainpower,
        indexed_by< N(byuser), const_mem_fun< brainpower, account_name, &brainpower::get_user >>,
        indexed_by< N(power), const_mem_fun< brainpower, uint64_t, &brainpower::get_power >>
    > brainpwrtbl;

    // test table
    // @abi table
    typedef eosio::multi_index<N(testtbl), teststruct> testtbl;

    // stake table
    // @abi table
    typedef eosio::multi_index<N(staketbl), stake,
        indexed_by< N(byuser), const_mem_fun<stake, account_name, &stake::get_user >>,
        indexed_by< N(byuser64t), const_mem_fun< stake, uint64_t, &stake::get_user64t >>
    > staketbl;

    // rewards history table
    // @abi table
    typedef eosio::multi_index<N(rewardstbl), rewardhistory,
        indexed_by< N(byuser), const_mem_fun<rewardhistory, account_name, &rewardhistory::get_user>>,
        indexed_by< N(byuser64t), const_mem_fun<rewardhistory, uint64_t, &rewardhistory::get_user64t >>,
        indexed_by< N(byfinalper), const_mem_fun<rewardhistory, uint64_t, &rewardhistory::get_finalize_period >>,
        indexed_by< N(byproposal), const_mem_fun<rewardhistory, uint64_t, &rewardhistory::get_proposal >>
    > rewardstbl;


    // ==================================================
    // =================================================
    // ==================================================
    // HELPER FUNCTIONS

    bool isnewuser (const account_name& _thisaccount);


public:
    eparticlectr(account_name self) : contract(self) {};

    uint64_t getiqbalance( account_name from );
    uint64_t swapEndian64( uint64_t input );

    //  ==================================================
    //  ==================================================
    //  ==================================================
    // ABI Functions

    void propose( account_name proposer,
                  ipfshash_t& proposed_article_hash,
                  ipfshash_t& old_article_hash,
                  ipfshash_t& grandparent_hash );

    void procrewards( uint64_t reward_period );

    void votebyhash ( account_name voter,
                      ipfshash_t& proposed_article_hash,
                      bool approve,
                      uint64_t amount );

    void finalize( uint64_t proposal_id );

    void fnlbyhash( ipfshash_t& proposal_hash );

    void brainmeart( account_name staker,
                  uint64_t amount );

    void brainclmid( account_name claimant,
                  uint64_t stakeid );

    void updatewiki( ipfshash_t& current_hash,
                  ipfshash_t& parent_hash );

    void withdraw( account_name from );
};

eosio::key256 eparticlectr::ipfs_to_key256(const ipfshash_t& input) {
    key256 returnKey;
    if (input == "") {
        returnKey = key256::make_from_word_sequence<uint64_t>(0ULL, 0ULL, 0ULL, 0ULL);
    }
    else {
        // This is needed for indexing since indexes cannot be done by strings, only max key256's, for now...
        uint64_t p1 = eosio::string_to_name(input.substr(0, 12).c_str());
        uint64_t p2 = eosio::string_to_name(input.substr(13, 24).c_str());
        uint64_t p3 = eosio::string_to_name(input.substr(25, 36).c_str());
        uint64_t p4 = eosio::string_to_name(input.substr(37, 45).c_str());
        returnKey = key256::make_from_word_sequence<uint64_t>(p1, p2, p3, p4);
    }
    return returnKey;
}

// This is until secondary keys get fixed with cleos get table :)
uint64_t eparticlectr::ipfs_to_uint64_trunc(const ipfshash_t& input) {
    ipfshash_t newHash = input;
    char chars[] = "6789";
    for (unsigned int i = 0; i < strlen(chars); ++i)
    {
       newHash.erase(std::remove(newHash.begin(), newHash.end(), chars[i]), newHash.end());
    }
    ipfshash_t truncatedHash = newHash.substr(2,12);
    transform(truncatedHash.begin(), truncatedHash.end(), truncatedHash.begin(), ::tolower);
    const char* cstringedMiniHash = truncatedHash.c_str();
    print(cstringedMiniHash, "\n");
    uint64_t hashNumber = eosio::string_to_name(cstringedMiniHash);
    print("Before: ", hashNumber, "\n");
    hashNumber = hashNumber % 9007199254740990; // Max safe javascript integer
    print("After: ", hashNumber, "\n");
    return(hashNumber);
}
