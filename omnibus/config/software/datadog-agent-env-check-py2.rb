name 'datadog-agent-env-check-py2'

description "Execute pip check on the python environment of the agent to make sure everything is compatible"

# Run the check after all the definitions touching the python environment of the agent.
dependency "datadog-pip-py2"
dependency "datadog-agent"
dependency "datadog-agent-integrations-py2"

build do
    # Run pip check to make sure the agent's python environment is clean, all the dependencies are compatible
    py2pip "check"
end
