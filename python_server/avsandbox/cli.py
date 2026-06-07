import argparse
import logging
import sys
from typing import Dict, Any

logger = logging.getLogger(__name__)


def train_entrypoint():
    parser = argparse.ArgumentParser(description="AVSandbox RL Training")
    parser.add_argument("--num-workers", type=int, default=4)
    parser.add_argument("--num-envs-per-worker", type=int, default=2)
    parser.add_argument("--framework", type=str, default="torch", choices=["torch", "tf2"])
    parser.add_argument("--algo", type=str, default="PPO", choices=["PPO", "SAC", "DQN", "APPO"])
    parser.add_argument("--train-iterations", type=int, default=1000)
    parser.add_argument("--ue5-host", type=str, default="127.0.0.1")
    parser.add_argument("--ue5-port", type=int, default=9000)
    parser.add_argument("--python-host", type=str, default="127.0.0.1")
    parser.add_argument("--python-port", type=int, default=9001)
    parser.add_argument("--map-path", type=str, default=None)
    parser.add_argument("--checkpoint-dir", type=str, default="./checkpoints")
    args = parser.parse_args()

    try:
        import ray
        from ray.rllib.algorithms.ppo import PPOConfig
        from ray.tune import TuneConfig
        from ray import air, tune
    except ImportError:
        logger.error("Ray/RLlib not installed. Run: pip install ray[rllib]")
        sys.exit(1)

    from avsandbox.env import AVSandboxEnv

    ray.init(ignore_reinit_error=True)

    env_config: Dict[str, Any] = {
        "ue5_listen_host": args.ue5_host,
        "ue5_listen_port": args.ue5_port,
        "python_listen_host": args.python_host,
        "python_listen_port": args.python_port,
        "ray_num_workers": args.num_workers,
        "ray_num_envs_per_worker": args.num_envs_per_worker,
        "ray_framework": args.framework,
        "open_drive_map_path": args.map_path,
    }

    if args.algo == "PPO":
        config = (
            PPOConfig()
            .environment(
                env=AVSandboxEnv,
                env_config=env_config,
            )
            .framework(args.framework)
            .rollouts(
                num_rollout_workers=args.num_workers,
                num_envs_per_worker=args.num_envs_per_worker,
            )
            .training(
                train_batch_size=4000,
                gamma=0.99,
                lr=3e-4,
                lambda_=0.95,
                clip_param=0.2,
                entropy_coeff=0.01,
            )
        )
    else:
        logger.error("Algorithm %s not yet supported", args.algo)
        sys.exit(1)

    tuner = tune.Tuner(
        args.algo,
        param_space=config.to_dict(),
        run_config=air.RunConfig(
            stop={"training_iteration": args.train_iterations},
            storage_path=args.checkpoint_dir,
        ),
    )

    results = tuner.fit()

    best_result = results.get_best_result(metric="episode_reward_mean", mode="max")
    if best_result:
        logger.info("Best checkpoint: %s", best_result.checkpoint)
        logger.info("Best reward: %s", best_result.metrics.get("episode_reward_mean"))

    ray.shutdown()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    train_entrypoint()
