public:
// note .. maybe this can be attached to another node structure?
// depends which works best for cache.
struct ItemPairs {
	struct Link {
		void set(BVHHandle h, void *ud) {
			handle = h;
			userdata = ud;
		}
		BVHHandle handle;
		void *userdata;
	};

	void clear() {
		num_pairs = 0;
		extended_pairs.reset();
		expanded_aabb = BOUNDS();
	}

	BOUNDS expanded_aabb;

	// maybe we can just use the number in the vector TODO
	int32_t num_pairs;
	LocalVector<Link> extended_pairs;

	void add_pair_to(BVHHandle h, void *p_userdata) {
		Link temp;
		temp.set(h, p_userdata);

		extended_pairs.push_back(temp);
		num_pairs++;
	}

	uint32_t find_pair_to(BVHHandle h) const {
		for (int n = 0; n < num_pairs; n++) {
			if (extended_pairs[n].handle == h) {
				return n;
			}
		}
		return uint32_t(-1);
	}

	bool contains_pair_to(BVHHandle h) const {
		return find_pair_to(h) != BVHCommon::INVALID;
	}

	// return success
	void *remove_pair_to(BVHHandle h) {
		void *userdata = nullptr;

		for (int n = 0; n < num_pairs; n++) {
			if (extended_pairs[n].handle == h) {
				userdata = extended_pairs[n].userdata;
				extended_pairs.remove_at_unordered(n);
				num_pairs--;
				break;
			}
		}

		return userdata;
	}

	// experiment : scale the pairing expansion by the number of pairs.
	// when the number of pairs is high, the density is high and a lower collision margin is better.
	// when there are few local pairs, a larger margin is more optimal.
	real_t scale_expansion_margin(real_t p_margin) const {
		real_t x = real_t(num_pairs) * (1.0 / 9.0);
		x = MIN(x, 1.0);
		x = 1.0 - x;
		return p_margin * x;
	}
};
