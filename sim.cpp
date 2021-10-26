//usr/bin/env g++ -xc++ -std=c++14 -Ofast $0 -o bin_${0##*/} ; ./bin_${0##*/} ; rm bin_${0##*/} ; exit $s ;

#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <random>
#include <algorithm>

class frequency_scaling_constant {
public:
    static std::string name() { return {"Freq_scaling=OFF,           "}; }

    static int f(int jobs_current) { return 1; }
};

template<int n>
class frequency_scaling_linear {
public:
    static std::string name() { return std::string("Freq_scaling: j_rem*") + std::to_string(n) + ",      "; }

    static int f(int jobs_current) { return n * jobs_current; }
};

template<int n>
class frequency_scaling_invexp {
public:
    static std::string name() { return std::string("Freq_scaling: j_rem**(1/") + std::to_string(n) + "), "; }

    static int f(int jobs_current) { return std::pow(jobs_current, 1. / n); }
};


class ps_scheduling {
public:
    static std::string name() { return "PS,  "; }

    template<class frequency_scaling_policy>
    static double compute_finish_time(int k, const std::vector<double> &work_sizes) {
        double acc = work_sizes[0] * static_cast<double>(work_sizes.size()) / frequency_scaling_policy::f(work_sizes.size());
        for (size_t i = 1; i < k; ++i) {
            acc += (work_sizes[i] - work_sizes[i - 1]) * (static_cast<double>(work_sizes.size()) - i + 1) / frequency_scaling_policy::f(work_sizes.size() - i + 1);
        }
        return acc;
    }
};

class fsp_scheduling {
public:
    static std::string name() { return "FSP, "; }

    template<class frequency_scaling_policy>
    static double compute_finish_time(int k, const std::vector<double> &work_sizes) {
        double acc = 0;
        for (size_t i = 0; i < k; ++i) {
            acc += work_sizes[i] / frequency_scaling_policy::f(work_sizes.size() - i + 1);
        }
        return acc;
    }
};

template<int alpha>
class power_regular {
public:
    static std::string name() { return std::string("Power: r**") + std::to_string(alpha) + ", "; }

    static double power(double rate) {
        return std::pow(rate, alpha);
    }
};


template<class scheduling_policy_original>
class turbocharging_policy_naive {
public:
    static std::string name() { return "Turbocharging ON,     "; }

    template<class scheduling_policy_target, class frequency_scaling_policy>
    static double turbocharging_rate(const std::vector<double> &work_sizes) {
        double old_end_time = scheduling_policy_original::template compute_finish_time<frequency_scaling_policy>(work_sizes.size(), work_sizes);
        double new_end_time = scheduling_policy_target::template compute_finish_time<frequency_scaling_policy>(work_sizes.size(), work_sizes);
        return std::max(1., new_end_time / old_end_time);
    }
};

template<class scheduling_policy_original>
class turbocharging_policy_strong {
public:
    static std::string name() { return "Turbocharging Strong, "; }

    template<class scheduling_policy_target, class frequency_scaling_policy>
    static double turbocharging_rate(const std::vector<double> &work_sizes) {
        double max_rate_needed = 1;
        for (int i = 0; i <= work_sizes.size(); ++i) {
            double old_end_time = scheduling_policy_original::template compute_finish_time<frequency_scaling_policy>(i, work_sizes);
            double new_end_time = scheduling_policy_target::template compute_finish_time<frequency_scaling_policy>(i, work_sizes);
            max_rate_needed = std::max(max_rate_needed, new_end_time / old_end_time);
        }
        return max_rate_needed;
    }
};


template<class = void>
class turbocharging_policy_off {
public:
    static std::string name() { return "Turbocharging OFF,    "; }

    template<class, class>
    static double turbocharging_rate(const std::vector<double> &work_sizes) {
        return 1;
    }
};


template<class scheduling_policy, class frequency_scaling_policy, class turbocharging_policy = turbocharging_policy_off<>, class power_policy = power_regular<2>>
class simulator {
private:
    static double get_turbo_rate(const std::vector<double> &work_sizes) {
        return turbocharging_policy::template turbocharging_rate<scheduling_policy, frequency_scaling_policy>(work_sizes);
    }

    static std::vector<double> get_departure_times(const std::vector<double> &work_sizes) {
        std::vector<double> out(work_sizes.size() + 1, 0);
        for (int i = 1; i <= work_sizes.size(); ++i) {
            out[i] = scheduling_policy::template compute_finish_time<frequency_scaling_policy>(i, work_sizes);
        }
        return out;
    }

    static double compute_energy_consumed(const std::vector<double> &departure_times, double turbo_rate) {
        double acc = 0;//departure_times[0] * power_policy::power(turbo_rate * frequency_scaling_policy::f(departure_times.size() - 1)); // First value special case
        for (int i = 1; i < departure_times.size(); ++i) {
            auto left = departure_times.size() - i;
            auto cpu_rate = frequency_scaling_policy::f(left);
            acc += (departure_times[i] - departure_times[i - 1]) * power_policy::power(turbo_rate * cpu_rate);
        }
        return acc;
    }

    static double mean_response_time(const std::vector<double> &departure_times, double turbo_rate) {
        auto sum = std::accumulate(departure_times.begin(), departure_times.end(), 0.);
        return sum / (turbo_rate * static_cast<double>(departure_times.size()));
    }


public:
    static void simulate(const std::vector<double> &work_sizes) {

        std::ios_base::fmtflags f(std::cout.flags());


        auto turbo_rate = get_turbo_rate(work_sizes);
        auto departures = get_departure_times(work_sizes);
        auto energy = compute_energy_consumed(departures, turbo_rate);
        std::cout << scheduling_policy::name() << frequency_scaling_policy::name() << turbocharging_policy::name() << power_policy::name();
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Turbocharging rate: " << turbo_rate << ", ";
        std::cout << "Energy Consumed: " << std::setw(5) << energy << ", ";
        std::cout << "Mean Response Time: " << mean_response_time(work_sizes, turbo_rate) << ", ";
        std::cout << "Departure Times: ";
        for (int i = 1; i <= work_sizes.size(); ++i) {
            std::cout << std::setw(5) << scheduling_policy::template compute_finish_time<frequency_scaling_policy>(i, work_sizes) / turbo_rate << ", ";
        }
        std::cout << std::endl;

        std::cout.flags(f);

    }
};


std::vector<double> generate_random_jobs(int n) {
    auto out = std::vector<double>(n, 0);
    static std::mt19937 engine(std::random_device{}());
    std::generate(out.begin(), out.end(), [&]() {
        static std::uniform_int_distribution<int> distribution(0, 100);
        return distribution(engine);
    });
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<double> generate_multiplicative_jobs(int n) {
    auto out = std::vector<double>(n, 0);
    for (int i = 0; i < n; ++i) {
        out[i] = 1 << i;
    }
    return out;
}


void test_policies(const std::vector<double> &works) {

    std::cout << std::endl;
    for (const auto &i: works)
        std::cout << i << ' ';
    std::cout << std::endl;


    // PS regular, alpha 1
    simulator<
            ps_scheduling,
            frequency_scaling_invexp<1>,
            turbocharging_policy_off<>,
            power_regular<1>
    >::simulate(works);

    // PS regular, alpha 2
    simulator<
            ps_scheduling,
            frequency_scaling_invexp<2>,
            turbocharging_policy_off<>,
            power_regular<2>
    >::simulate(works);

    // FSP no turbo, alpha = 1
    simulator<
            fsp_scheduling,
            frequency_scaling_invexp<1>,
            turbocharging_policy_off<>,
            power_regular<1>
    >::simulate(works);

    // FSP no turbo, alpha = 2
    simulator<
            fsp_scheduling,
            frequency_scaling_invexp<2>,
            turbocharging_policy_off<>,
            power_regular<2>
    >::simulate(works);


    // FSP turbo, alpha = 1
    simulator<
            fsp_scheduling,
            frequency_scaling_invexp<1>,
            turbocharging_policy_naive<ps_scheduling>,
            power_regular<1>
    >::simulate(works);

    // FSP turbo, alpha = 2
    simulator<
            fsp_scheduling,
            frequency_scaling_invexp<2>,
            turbocharging_policy_naive<ps_scheduling>,
            power_regular<2>
    >::simulate(works);

    // FSP Strong dominance alpha 1
    simulator<
            fsp_scheduling,
            frequency_scaling_invexp<1>,
            turbocharging_policy_strong<ps_scheduling>,
            power_regular<1>
    >::simulate(works);

    // FSP Strong dominance alpha 2
    simulator<
            fsp_scheduling,
            frequency_scaling_invexp<2>,
            turbocharging_policy_strong<ps_scheduling>,
            power_regular<2>
    >::simulate(works);

    // FSP Strong dominance alpha 3
    simulator<
            fsp_scheduling,
            frequency_scaling_invexp<3>,
            turbocharging_policy_strong<ps_scheduling>,
            power_regular<3>
    >::simulate(works);


    simulator<ps_scheduling, frequency_scaling_constant>::simulate(works);
    simulator<fsp_scheduling, frequency_scaling_constant>::simulate(works);

}

int main() {
    test_policies({10, 10, 10, 10, 10, 10});
    test_policies({10, 10, 10, 10, 10, 100000});
    test_policies({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    test_policies(generate_random_jobs(10));
    test_policies(generate_multiplicative_jobs(10));
    return 0;
}
